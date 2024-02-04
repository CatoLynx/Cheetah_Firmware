#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "esp_http_client.h"
#include "mbedtls/base64.h"

#include "remote_poll.h"
#include "macros.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_nvs.h"


#define LOG_TAG "Poll"


static TaskHandle_t rp_task_handle;
nvs_handle_t rp_nvs_handle;
char* pollUrl = NULL;
char* pollToken = NULL;
uint16_t pollInterval = 0;
uint8_t pollUrlInited = 0;
uint8_t pollTokenInited = 0;
uint32_t last_update_id = 0;

// Dynamic array holding the current list of buffers
rp_buffer_list_entry_t* rp_buffers = NULL;
uint8_t rp_num_buffers = 0;
uint8_t rp_cur_buffer = 0;
bool rp_restart_cycle = false;

// Last switch / update times
uint64_t rp_last_switch = 0;
uint64_t rp_last_update = 0;

uint8_t* output_buffer;
size_t output_buffer_size = 0;

extern uint8_t wifi_gotIP;


esp_err_t remote_poll_http_event_handler(esp_http_client_event_t *evt) {
    static char *resp_buf;
    static int resp_len;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR: {
            ESP_LOGD(LOG_TAG, "HTTP_EVENT_ERROR");
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
                    resp_buf = (char *) malloc(esp_http_client_get_content_length(evt->client));
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

            esp_err_t ret = remote_poll_process_response(json);
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

void remote_poll_init(nvs_handle_t* nvsHandle, uint8_t* outBuf, size_t bufSize) {
    ESP_LOGI(LOG_TAG, "Initializing remote poll");
    rp_nvs_handle = *nvsHandle;
    output_buffer = outBuf;
    output_buffer_size = bufSize;

    remote_poll_deinit();

    esp_err_t ret = nvs_get_u16(*nvsHandle, "poll_interval", &pollInterval);
    if (ret != ESP_OK) pollInterval = 0;

    pollUrl = get_string_from_nvs(nvsHandle, "poll_url");
    if (pollUrl != NULL) pollUrlInited = 1;

    pollToken = get_string_from_nvs(nvsHandle, "poll_token");
    if (pollToken != NULL) pollTokenInited = 1;

    if (pollInterval != 0 && pollUrl != NULL && pollToken != NULL) {
        if (strlen(pollUrl) != 0 && strlen(pollToken) != 0) {
            ESP_LOGI(LOG_TAG, "Starting remote poll task");
            xTaskCreatePinnedToCore(remote_poll_task, "remote_poll", 4096, NULL, 5, &rp_task_handle, 0);
        }
    }
}

void remote_poll_deinit() {
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

void remote_poll_task(void* arg) {
    while (1) {
        uint64_t now = esp_timer_get_time(); // Microseconds!

        // Switch buffer if necessary
        if (rp_num_buffers > 0) {
            if (rp_cur_buffer >= rp_num_buffers) rp_cur_buffer = 0; // In case rp_num_buffers got smaller
            if (rp_restart_cycle == true) {
                ESP_LOGI(LOG_TAG, "Restarting cycle");
                rp_cur_buffer = 0;
            }
            if (rp_restart_cycle == true || rp_last_switch == 0 || now - rp_last_switch >= rp_buffers[rp_cur_buffer].duration * 1000000) {
                rp_last_switch = now;
                if (!(rp_restart_cycle == true || rp_last_switch == 0)) rp_cur_buffer++;
                rp_restart_cycle = false;
                if (rp_cur_buffer >= rp_num_buffers) rp_cur_buffer = 0;
                ESP_LOGI(LOG_TAG, "Switching to buffer %d", rp_cur_buffer);
                memcpy(output_buffer, rp_buffers[rp_cur_buffer].buffer, output_buffer_size);
            }
        }

        // Update if necessary
        if (rp_last_update == 0 || now - rp_last_update >= pollInterval * 1000000) {
            rp_last_update = now;
            if (wifi_gotIP) remote_poll_send_request();
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void remote_poll_send_request() {
    esp_http_client_config_t config = {
        .event_handler = remote_poll_http_event_handler,
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
        ESP_LOGI(LOG_TAG, "HTTP GET Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(LOG_TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        // sprintf((char*)output_buffer, "GET FAILED %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

esp_err_t remote_poll_process_response(cJSON* json) {
    /*
    Expected JSON schema:
    {
        "restartCycle": false,
        "buffers": [
            {
                "duration": <duration in seconds>,
                "buffer": <base64-encoded buffer>
            },
            {
                "duration": <duration in seconds>,
                "buffer": <base64-encoded buffer>
            }
        ]
    }

    On error:
    {
        "error": "Some error here"
    }
    */

    ESP_LOGD(LOG_TAG, "Processing response");
    if (!cJSON_IsObject(json)) return ESP_FAIL;

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
        rp_restart_cycle = cJSON_IsTrue(field_restart);
    } else {
        rp_restart_cycle = false;
    }

    cJSON* buffers_arr = cJSON_GetObjectItem(json, "buffers");
    if (!cJSON_IsArray(buffers_arr)) return ESP_FAIL;
    uint16_t numBuffers = cJSON_GetArraySize(buffers_arr);
    if (numBuffers > 255) {
        ESP_LOGE(LOG_TAG, "Got more than 255 buffers, aborting");
        return ESP_FAIL;
    }

    // Free individual buffer arrays and the overall array
    for (uint8_t i = 0; i < rp_num_buffers; i++) {
        free(rp_buffers[i].buffer);
    }
    free(rp_buffers);

    // Allocate new buffers
    ESP_LOGD(LOG_TAG, "Allocating %d buffers", numBuffers);
    rp_num_buffers = numBuffers;
    rp_buffers = malloc(rp_num_buffers * sizeof(rp_buffer_list_entry_t));
    memset(rp_buffers, 0x00, rp_num_buffers * sizeof(rp_buffer_list_entry_t));
    for (uint16_t i = 0; i < numBuffers; i++) {
        rp_buffers[i].buffer = malloc(output_buffer_size);
    }

    // Populate new buffers
    for (uint8_t i = 0; i < rp_num_buffers; i++) {
        size_t b64_len = 0;
        cJSON* item = cJSON_GetArrayItem(buffers_arr, i);

        cJSON* duration_field = cJSON_GetObjectItem(item, "duration");
        rp_buffers[i].duration = cJSON_GetNumberValue(duration_field);

        cJSON* buffer_field = cJSON_GetObjectItem(item, "buffer");
        char* buffer_str = cJSON_GetStringValue(buffer_field);
        size_t buffer_str_len = strlen(buffer_str);
        unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
        int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
        if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
            // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
            // because this will always be returned when checking size
            return ESP_FAIL;
        } else {
            b64_len = 0;
            result = mbedtls_base64_decode(rp_buffers[i].buffer, output_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
            if (result != 0) return ESP_FAIL;
        }
    }

    return ESP_OK;
}