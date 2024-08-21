#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "mbedtls/base64.h"

#include "playlist.h"
#include "macros.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_nvs.h"


#define LOG_TAG "Playlist"


static TaskHandle_t pl_task_handle;
static nvs_handle_t pl_nvs_handle;
static char* pollUrl = NULL;
static char* pollToken = NULL;
static uint16_t pollInterval = 0;
static uint8_t pollUrlInited = 0;
static uint8_t pollTokenInited = 0;

// Dynamic array holding the current list of buffers
static pl_buffer_list_entry_t* pl_buffers = NULL;
static uint16_t pl_num_buffers = 0;
static uint16_t pl_cur_buffer = 0;
static bool pl_restart_cycle = false;

// Last switch / update times
static uint64_t pl_last_switch = 0;
static uint64_t pl_last_update = 0;

static uint8_t* pixel_buffer;
static size_t pixel_buffer_size = 0;
static uint8_t* text_buffer;
static size_t text_buffer_size = 0;
static uint8_t* unit_buffer;
static size_t unit_buffer_size = 0;

extern uint8_t wifi_gotIP;


esp_err_t playlist_http_event_handler(esp_http_client_event_t *evt) {
    static char *resp_buf;
    static int resp_len;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_ERROR");
            break;
        }

        case HTTP_EVENT_REDIRECT: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_REDIRECT");
            break;
        }

        case HTTP_EVENT_ON_CONNECTED: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        }

        case HTTP_EVENT_HEADER_SENT: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        }

        case HTTP_EVENT_ON_HEADER: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        }

        case HTTP_EVENT_ON_DATA: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (resp_buf == NULL) {
                    resp_buf = (char *) heap_caps_malloc(esp_http_client_get_content_length(evt->client), MALLOC_CAP_SPIRAM);
                    resp_len = 0;
                    if (resp_buf == NULL) {
                        ESP_LOGE(LOG_TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                memcpy(resp_buf + resp_len, evt->data, evt->data_len);
                resp_len += evt->data_len;
            }

            break;
        }

        case HTTP_EVENT_ON_FINISH: {
            cJSON* json;
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_ON_FINISH");
            if (resp_buf != NULL) {
                json = cJSON_Parse(resp_buf);
                free(resp_buf);
                resp_buf = NULL;
            } else {
                return ESP_FAIL;
            }
            resp_len = 0;

            esp_err_t ret = playlist_process_response(json);
            if (ret != ESP_OK) {
                //memset(output_buffer, 0x00, output_buffer_size);
                ESP_LOGE(LOG_TAG,  "Error");
                cJSON_Delete(json);
                return ret;
            }
            cJSON_Delete(json);

            break;
        }

        case HTTP_EVENT_DISCONNECTED: {
            ESP_LOGI(LOG_TAG, "HTTP_EVENT_DISCONNECTED");
            if (resp_buf != NULL) {
                free(resp_buf);
                resp_buf = NULL;
            }
            resp_len = 0;
            break;
        }
    }
    return ESP_OK;
}

void playlist_init(nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, uint8_t* textBuf, size_t textBufSize, uint8_t* unitBuf, size_t unitBufSize) {
    ESP_LOGI(LOG_TAG, "Initializing playlist");
    pl_nvs_handle = *nvsHandle;
    pixel_buffer = pixBuf;
    pixel_buffer_size = pixBufSize;
    text_buffer = textBuf;
    text_buffer_size = textBufSize;
    unit_buffer = unitBuf;
    unit_buffer_size = unitBufSize;

    playlist_deinit();

    esp_err_t ret = nvs_get_u16(*nvsHandle, "pl_poll_intvl", &pollInterval);
    if (ret != ESP_OK) pollInterval = 0;

    pollUrl = get_string_from_nvs(nvsHandle, "pl_poll_url");
    if (pollUrl != NULL) pollUrlInited = 1;

    pollToken = get_string_from_nvs(nvsHandle, "pl_poll_token");
    if (pollToken != NULL) pollTokenInited = 1;

    if (pollInterval != 0 && pollUrl != NULL && pollToken != NULL) {
        if (strlen(pollUrl) != 0 && strlen(pollToken) != 0) {
            ESP_LOGI(LOG_TAG, "Starting playlist task");
            xTaskCreatePinnedToCore(playlist_task, "playlist", 4096, NULL, 5, &pl_task_handle, 0);
        }
    }
}

void playlist_deinit() {
    // Free allocated memory
    if (pollUrlInited) {
        free(pollUrl);
        pollUrl = NULL;
        pollUrlInited = 0;
    }
    if (pollTokenInited) {
        free(pollToken);
        pollToken = NULL;
        pollTokenInited = 0;
    }
}

void playlist_task(void* arg) {
    while (1) {
        uint64_t now = esp_timer_get_time(); // Microseconds!

        // Switch buffer if necessary
        if (pl_num_buffers > 0) {
            if (pl_cur_buffer >= pl_num_buffers) pl_cur_buffer = 0; // In case pl_num_buffers got smaller
            if (pl_restart_cycle == true) {
                ESP_LOGI(LOG_TAG, "Restarting cycle");
                pl_cur_buffer = 0;
            }
            if (pl_restart_cycle == true || pl_last_switch == 0 || now - pl_last_switch >= pl_buffers[pl_cur_buffer].duration * 1000000) {
                if (!(pl_restart_cycle == true || pl_last_switch == 0)) pl_cur_buffer++;
                pl_last_switch = now;
                pl_restart_cycle = false;
                if (pl_cur_buffer >= pl_num_buffers) pl_cur_buffer = 0;

                // If the playlist input is disabled in NVS with this flag,
                // It'll keep running in the background, but not outputting anything
                // This gets checked every loop cycle so that it takes immediate effect
                uint8_t active = 0;
                nvs_get_u8(pl_nvs_handle, "playlist_active", &active);
                if (active) {
                    ESP_LOGD(LOG_TAG, "Switching to buffer %d", pl_cur_buffer);
                    if (pl_buffers[pl_cur_buffer].pixelBuffer != NULL) memcpy(pixel_buffer, pl_buffers[pl_cur_buffer].pixelBuffer, pixel_buffer_size);
                    if (pl_buffers[pl_cur_buffer].textBuffer  != NULL) memcpy(text_buffer,  pl_buffers[pl_cur_buffer].textBuffer,  text_buffer_size);
                    if (pl_buffers[pl_cur_buffer].unitBuffer  != NULL) memcpy(unit_buffer,  pl_buffers[pl_cur_buffer].unitBuffer,  unit_buffer_size);
                }
            }
        }

        // Update if necessary
        if (pl_last_update == 0 || now - pl_last_update >= pollInterval * 1000000) {
            if (wifi_gotIP) {
                playlist_send_request();
                pl_last_update = now;
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void playlist_send_request() {
    esp_http_client_config_t config = {
        .event_handler = playlist_http_event_handler,
        .disable_auto_redirect = false,
        .url = pollUrl
    };
    cJSON* json;
    char* post_data;
    esp_http_client_handle_t client = esp_http_client_init(&config);

    json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "token", pollToken);
    post_data = cJSON_Print(json);
    ESP_LOGV(LOG_TAG, "POST Data: %s", post_data);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    cJSON_Delete(json);
    cJSON_free(post_data);

    if (err == ESP_OK) {
        ESP_LOGI(LOG_TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(LOG_TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        // sprintf((char*)output_buffer, "GET FAILED %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

esp_err_t playlist_process_response(cJSON* json) {
    /*
    Expected JSON schema (only non-null buffers will be updated):
    {
        "restartCycle": false,
        "buffers": [
            {
                "duration": <duration in seconds>,
                "buffer": {
                    "pixel": <base64-encoded buffer>,
                    "text": <base64-encoded buffer>,
                    "unit": null
                }
            },
            {
                "duration": <duration in seconds>,
                "buffer": {
                    "pixel": null,
                    "text": <base64-encoded buffer>,
                    "unit": null
                }
            }
        ]
    }

    On error:
    {
        "error": "Some error here"
    }
    */

    ESP_LOGD(LOG_TAG, "Processing response");
    if (!cJSON_IsObject(json)) {
        ESP_LOGE(LOG_TAG, "Did not receive a valid JSON object");
        return ESP_FAIL;
    }

    cJSON* field_error = cJSON_GetObjectItem(json, "error");
    if (field_error != NULL) {
        char* error_str = cJSON_GetStringValue(field_error);
        ESP_LOGE(LOG_TAG, "Poll API Error: %s", error_str);
        return ESP_FAIL;
    }

    // If this is true, the next cycle will immediately begin and restart at buffer 0
    // Useful if the newly received message should be displayed immediately
    cJSON* field_restart = cJSON_GetObjectItem(json, "restartCycle");
    if (field_restart != NULL) {
        pl_restart_cycle = cJSON_IsTrue(field_restart);
    } else {
        pl_restart_cycle = false;
    }

    cJSON* buffers_arr = cJSON_GetObjectItem(json, "buffers");
    if (!cJSON_IsArray(buffers_arr)) {
        ESP_LOGE(LOG_TAG, "'buffers' is not an array'");
        return ESP_FAIL;
    }
    uint16_t numBuffers = cJSON_GetArraySize(buffers_arr);
    if (numBuffers > MAX_NUM_BUFFERS) {
        ESP_LOGE(LOG_TAG, "Got more than %d buffers, aborting", MAX_NUM_BUFFERS);
        return ESP_FAIL;
    }

    // Free individual buffer arrays and the overall array
    for (uint16_t i = 0; i < pl_num_buffers; i++) {
        free(pl_buffers[i].pixelBuffer);
        free(pl_buffers[i].textBuffer);
        free(pl_buffers[i].unitBuffer);
    }
    free(pl_buffers);

    // Allocate new buffers
    ESP_LOGD(LOG_TAG, "Allocating %d buffers", numBuffers);
    pl_num_buffers = numBuffers;
    pl_buffers = malloc(pl_num_buffers * sizeof(pl_buffer_list_entry_t));
    memset(pl_buffers, 0x00, pl_num_buffers * sizeof(pl_buffer_list_entry_t));

    // Populate new buffers
    for (uint16_t i = 0; i < pl_num_buffers; i++) {
        size_t b64_len = 0;
        cJSON* item = cJSON_GetArrayItem(buffers_arr, i);

        cJSON* duration_field = cJSON_GetObjectItem(item, "duration");
        pl_buffers[i].duration = cJSON_GetNumberValue(duration_field);

        cJSON* buffer_field = cJSON_GetObjectItem(item, "buffer");

        cJSON* pixbuf_field = cJSON_GetObjectItem(buffer_field, "pixel");
        if (pixbuf_field != NULL && !cJSON_IsNull(pixbuf_field)) {
            char* buffer_str = cJSON_GetStringValue(pixbuf_field);
            size_t buffer_str_len = strlen(buffer_str);
            unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
            int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
            if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
                // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
                // because this will always be returned when checking size
                ESP_LOGE(LOG_TAG, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in pixbuf");
                return ESP_FAIL;
            } else {
                b64_len = 0;
                pl_buffers[i].pixelBuffer = heap_caps_malloc(pixel_buffer_size, MALLOC_CAP_SPIRAM);
                result = mbedtls_base64_decode(pl_buffers[i].pixelBuffer, pixel_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
                if (result != 0) {
                    ESP_LOGE(LOG_TAG, "mbedtls_base64_decode() failed for pixbuf");
                    return ESP_FAIL;
                }
            }
        }
        
        cJSON* textbuf_field = cJSON_GetObjectItem(buffer_field, "text");
        if (textbuf_field != NULL && !cJSON_IsNull(textbuf_field)) {
            char* buffer_str = cJSON_GetStringValue(textbuf_field);
            size_t buffer_str_len = strlen(buffer_str);
            unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
            int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
            if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
                // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
                // because this will always be returned when checking size
                ESP_LOGE(LOG_TAG, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in textbuf");
                return ESP_FAIL;
            } else {
                b64_len = 0;
                pl_buffers[i].textBuffer = malloc(text_buffer_size);
                result = mbedtls_base64_decode(pl_buffers[i].textBuffer, text_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
                if (result != 0) {
                    ESP_LOGE(LOG_TAG, "mbedtls_base64_decode() failed for textbuf");
                    return ESP_FAIL;
                }
            }
        }
        
        cJSON* unitbuf_field = cJSON_GetObjectItem(buffer_field, "unit");
        if (unitbuf_field != NULL && !cJSON_IsNull(unitbuf_field)) {
            char* buffer_str = cJSON_GetStringValue(unitbuf_field);
            size_t buffer_str_len = strlen(buffer_str);
            unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
            int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
            if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
                // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
                // because this will always be returned when checking size
                ESP_LOGE(LOG_TAG, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in unitbuf");
                return ESP_FAIL;
            } else {
                b64_len = 0;
                pl_buffers[i].unitBuffer = malloc(unit_buffer_size);
                result = mbedtls_base64_decode(pl_buffers[i].unitBuffer, unit_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
                if (result != 0) {
                    ESP_LOGE(LOG_TAG, "mbedtls_base64_decode() failed for unitbuf");
                    return ESP_FAIL;
                }
            }
        }
    }

    return ESP_OK;
}