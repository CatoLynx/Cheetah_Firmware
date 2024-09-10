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
static char* playlistFile = NULL;
static uint16_t pollInterval = 0;
static uint8_t pl_save_to_file = 0;
static uint8_t pollUrlInited = 0;
static uint8_t pollTokenInited = 0;
static uint8_t playlistFileInited = 0;
static uint8_t pollUrlValid = 0;
static uint8_t pollTokenValid = 0;
static uint8_t playlistFileValid = 0;

// Dynamic array holding the current list of buffers
static pl_buffer_list_entry_t* pl_buffers = NULL;
static uint16_t pl_num_buffers = 0;
static uint16_t pl_cur_buffer = 0;
static bool pl_restart_cycle = false;
static pl_mode_t pl_mode = PL_SEQUENTIAL;

// Last switch / update times
static uint64_t pl_last_switch = 0;
static uint64_t pl_last_update = 0;

static uint8_t* pixel_buffer;
static size_t pixel_buffer_size = 0;
static uint8_t* text_buffer;
static size_t text_buffer_size = 0;
static uint8_t* unit_buffer;
static size_t unit_buffer_size = 0;

extern uint8_t wifi_gotIP, eth_gotIP;

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static uint8_t* pl_brightness = NULL;
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
static cJSON** shader_data = NULL;
static uint8_t* shader_data_deletable = NULL;
#endif

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
static cJSON** transition_data = NULL;
static uint8_t* transition_data_deletable = NULL;
#endif

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
static cJSON** effect_data = NULL;
static uint8_t* effect_data_deletable = NULL;
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
static cJSON** bitmap_generator_data = NULL;
static uint8_t* bitmap_generator_data_deletable = NULL;
#endif


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

            esp_err_t ret = playlist_process_json(json);
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

    ret = nvs_get_u8(*nvsHandle, "pl_save_to_file", &pl_save_to_file);
    if (ret != ESP_OK) pl_save_to_file = 0;

    pollUrl = get_string_from_nvs(nvsHandle, "pl_poll_url");
    if (pollUrl != NULL) {
        pollUrlInited = 1;
        if (strlen(pollUrl) != 0) pollUrlValid = 1;
    }

    pollToken = get_string_from_nvs(nvsHandle, "pl_poll_token");
    if (pollToken != NULL) {
        pollTokenInited = 1;
        if (strlen(pollToken) != 0) pollTokenValid = 1;
    }

    playlistFile = get_string_from_nvs(nvsHandle, "playlist_file");
    if (playlistFile != NULL) {
        playlistFileInited = 1;
        if (strlen(playlistFile) != 0) playlistFileValid = 1;
    }

    if ((pollInterval != 0 && pollUrlValid && pollTokenValid) || playlistFileValid) {
        ESP_LOGI(LOG_TAG, "Starting playlist task");
        xTaskCreatePinnedToCore(playlist_task, "playlist", 4096, NULL, 5, &pl_task_handle, 0);
    }
}

void playlist_deinit() {
    // Free allocated memory
    if (pollUrlInited) {
        free(pollUrl);
        pollUrl = NULL;
        pollUrlInited = 0;
        pollUrlValid = 0;
    }
    if (pollTokenInited) {
        free(pollToken);
        pollToken = NULL;
        pollTokenInited = 0;
        pollTokenValid = 0;
    }
    if (playlistFileInited) {
        free(playlistFile);
        playlistFile = NULL;
        playlistFileInited = 0;
        playlistFileValid = 0;
    }
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
void playlist_register_brightness(uint8_t* brightness) {
    pl_brightness = brightness;
}
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
void playlist_register_shaders(cJSON** shaderData, uint8_t* shaderDataDeletable) {
    shader_data = shaderData;
    shader_data_deletable = shaderDataDeletable;
}
#endif

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
void playlist_register_transitions(cJSON** transitionData, uint8_t* transitionDataDeletable) {
    transition_data = transitionData;
    transition_data_deletable = transitionDataDeletable;
}
#endif

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
void playlist_register_effects(cJSON** effectData, uint8_t* effectDataDeletable) {
    effect_data = effectData;
    effect_data_deletable = effectDataDeletable;
}
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
void playlist_register_bitmap_generators(cJSON** bitmapGeneratorData, uint8_t* bitmapGeneratorDataDeletable) {
    bitmap_generator_data = bitmapGeneratorData;
    bitmap_generator_data_deletable = bitmapGeneratorDataDeletable;
}
#endif

void playlist_next_buffer() {
    if (pl_mode == PL_SEQUENTIAL) {
        pl_cur_buffer++;
        if (pl_cur_buffer >= pl_num_buffers) pl_cur_buffer = 0;
    } else if (pl_mode == PL_RANDOM) {
        pl_cur_buffer = rand() % pl_num_buffers;
    }
}

void playlist_first_buffer() {
    if (pl_mode == PL_SEQUENTIAL) {
        pl_cur_buffer = 0;
    } else if (pl_mode == PL_RANDOM) {
        pl_cur_buffer = rand() % pl_num_buffers;
    }
}

void playlist_task(void* arg) {
    while (1) {
        uint64_t now = esp_timer_get_time(); // Microseconds!

        // Switch buffer if necessary
        if (pl_num_buffers > 0) {
            if (pl_cur_buffer >= pl_num_buffers) playlist_first_buffer(); // In case pl_num_buffers got smaller
            if (pl_restart_cycle == true) {
                ESP_LOGI(LOG_TAG, "Restarting cycle");
                playlist_first_buffer();
            }
            if (pl_restart_cycle == true || pl_last_switch == 0 || now - pl_last_switch >= pl_buffers[pl_cur_buffer].duration * 1000000) {
                if (!(pl_restart_cycle == true || pl_last_switch == 0)) playlist_next_buffer();
                pl_last_switch = now;
                pl_restart_cycle = false;

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

                    #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
                    if (pl_brightness != NULL && pl_buffers[pl_cur_buffer].brightness != -1) {
                        *pl_brightness = pl_buffers[pl_cur_buffer].brightness;
                    }
                    #endif

                    #if defined(CONFIG_DISPLAY_HAS_SHADERS)
                    if (shader_data != NULL && pl_buffers[pl_cur_buffer].updateShader) {
                        *shader_data = pl_buffers[pl_cur_buffer].shader;
                        *shader_data_deletable = 0; // This is taken care of during playlist update
                    }
                    #endif

                    #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
                    if (transition_data != NULL && pl_buffers[pl_cur_buffer].updateTransition) {
                        *transition_data = pl_buffers[pl_cur_buffer].transition;
                        *transition_data_deletable = 0; // This is taken care of during playlist update
                    }
                    #endif

                    #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
                    if (effect_data != NULL && pl_buffers[pl_cur_buffer].updateEffect) {
                        *effect_data = pl_buffers[pl_cur_buffer].effect;
                        *effect_data_deletable = 0; // This is taken care of during playlist update
                    }
                    #endif

                    #if defined(DISPLAY_HAS_PIXEL_BUFFER)
                    if (bitmap_generator_data != NULL && pl_buffers[pl_cur_buffer].updateBitmapGenerator) {
                        *bitmap_generator_data = pl_buffers[pl_cur_buffer].bitmapGenerator;
                        *bitmap_generator_data_deletable = 0; // This is taken care of during playlist update
                    }
                    #endif
                }
            }
        }

        // Update if necessary
        if (pl_last_update == 0 || now - pl_last_update >= pollInterval * 1000000) {
            if (pollUrlValid && pollTokenValid && (wifi_gotIP || eth_gotIP)) {
                playlist_update_from_http();
            } else if(playlistFileValid) {
                playlist_update_from_file();
            }
            pl_last_update = now;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void playlist_update_from_http() {
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

void playlist_update_from_file() {
    char file_path[21]; // "/spiffs/" + 8.3 filename + null
    snprintf(file_path, 21, "/spiffs/%s", playlistFile);
    ESP_LOGI(LOG_TAG, "Reading file: %s", file_path);
    FILE* file = fopen(file_path, "r");
    if (file == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to open file");
        return;
    }

    struct stat st;
    if (stat(file_path, &st) != 0) {
        ESP_LOGE(LOG_TAG, "Failed to stat file");
        return;
    }
    
    off_t file_size = st.st_size;
    char* json_str = heap_caps_malloc(file_size, MALLOC_CAP_SPIRAM);
    if (json_str == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to allocate memory for JSON buffer");
        return;
    }
    fread(json_str, 1, file_size, file);
    cJSON* json = cJSON_Parse(json_str);
    free(json_str);
    fclose(file);

    esp_err_t ret = playlist_process_json(json);
    if (ret != ESP_OK) {
        //memset(output_buffer, 0x00, output_buffer_size);
        ESP_LOGE(LOG_TAG, "Error");
        cJSON_Delete(json);
    }
    cJSON_Delete(json);
}

esp_err_t playlist_process_json(cJSON* json) {
    /*
    Expected JSON schema (only non-null buffers will be updated):
    {
        "buffers":  [{
                "buffer": {
                    "text": "Plaintext here"
                },
                "duration": 1,
                "brightness": 10,
                "effect": {
                    "effect": 1,
                    "params": {
                        "duration_avg_ms": 50,
                        "duration_spread_ms": 50,
                        "interval_avg_ms": 1000,
                        "interval_spread_ms": 1000,
                        "non_blank_only": true,
                        "probability": 2500
                    }
                }
            },
            {
                "buffer": {
                    "text": "Another text"
                },
                "duration": 1,
                "brightness": 255,
                "effect": null
            },
            {
                "buffer": {
                    "text_b64": "<Base64 encoded buffer>"
                },
                "duration": 1,
                "brightness": 255,
                "effect": null
            }],
        "playlistMode": "<random|sequential>",
        "restartCycle": false
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
        ESP_LOGE(LOG_TAG, "JSON error: %s", error_str);
        return ESP_FAIL;
    }

    cJSON* field_mode = cJSON_GetObjectItem(json, "playlistMode");
    if (field_mode != NULL) {
        char* mode_str = cJSON_GetStringValue(field_mode);
        if (strcmp(mode_str, "sequential") == 0) pl_mode = PL_SEQUENTIAL;
        else if (strcmp(mode_str, "random") == 0) pl_mode = PL_RANDOM;
    } else {
        pl_mode = PL_SEQUENTIAL;
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

        cJSON* brightness_field = cJSON_GetObjectItem(item, "brightness");
        if (brightness_field != NULL && !cJSON_IsNull(brightness_field)) {
            pl_buffers[i].brightness = cJSON_GetNumberValue(brightness_field);
        } else  {
            pl_buffers[i].brightness = -1;
        }

        // For shader, transition, effect and bitmap generator fields,
        // the rule is that an explicit null entry in the JSON
        // clears the respective configuration while an omitted entry
        // keeps the previous configuration.

        cJSON* shader_field = cJSON_GetObjectItem(item, "shader");
        if (pl_buffers[i].shader != NULL) cJSON_Delete(pl_buffers[i].shader);
        pl_buffers[i].updateShader = (shader_field != NULL);
        pl_buffers[i].shader = cJSON_Duplicate(shader_field, true);

        cJSON* transition_field = cJSON_GetObjectItem(item, "transition");
        if (pl_buffers[i].transition != NULL) cJSON_Delete(pl_buffers[i].transition);
        pl_buffers[i].updateTransition = (transition_field != NULL);
        pl_buffers[i].transition = cJSON_Duplicate(transition_field, true);

        cJSON* effect_field = cJSON_GetObjectItem(item, "effect");
        if (pl_buffers[i].effect != NULL) cJSON_Delete(pl_buffers[i].effect);
        pl_buffers[i].updateEffect = (effect_field != NULL);
        pl_buffers[i].effect = cJSON_Duplicate(effect_field, true);

        cJSON* bitmap_generator_field = cJSON_GetObjectItem(item, "bitmap_generator");
        if (pl_buffers[i].bitmapGenerator != NULL) cJSON_Delete(pl_buffers[i].bitmapGenerator);
        pl_buffers[i].updateBitmapGenerator = (bitmap_generator_field != NULL);
        pl_buffers[i].bitmapGenerator = cJSON_Duplicate(bitmap_generator_field, true);

        cJSON* buffer_field = cJSON_GetObjectItem(item, "buffer");

        cJSON* pixbuf_field = cJSON_GetObjectItem(buffer_field, "pixel");
        uint8_t pixbuf_b64 = 0;
        if (pixbuf_field == NULL || cJSON_IsNull(pixbuf_field)) {
            pixbuf_field = cJSON_GetObjectItem(buffer_field, "pixel_b64");
            pixbuf_b64 = 1;
        }
        if (pixbuf_field != NULL && !cJSON_IsNull(pixbuf_field)) {
            char* buffer_str = cJSON_GetStringValue(pixbuf_field);
            size_t buffer_str_len = strlen(buffer_str);
            if (pixbuf_b64) {
                unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
                int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
                if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
                    // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
                    // because this will always be returned when checking size
                    ESP_LOGE(LOG_TAG, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in pixbuf");
                    return ESP_FAIL;
                } else {
                    b64_len = 0;
                    pl_buffers[i].pixelBuffer = heap_caps_calloc(1, pixel_buffer_size, MALLOC_CAP_SPIRAM);
                    result = mbedtls_base64_decode(pl_buffers[i].pixelBuffer, pixel_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
                    if (result != 0) {
                        ESP_LOGE(LOG_TAG, "mbedtls_base64_decode() failed for pixbuf");
                        return ESP_FAIL;
                    }
                }
            } else {
                pl_buffers[i].pixelBuffer = heap_caps_calloc(1, pixel_buffer_size, MALLOC_CAP_SPIRAM);
                memcpy(pl_buffers[i].pixelBuffer, buffer_str, MIN(buffer_str_len, pixel_buffer_size));
            }
        }
        
        cJSON* textbuf_field = cJSON_GetObjectItem(buffer_field, "text");
        uint8_t textbuf_b64 = 0;
        if (textbuf_field == NULL || cJSON_IsNull(textbuf_field)) {
            textbuf_field = cJSON_GetObjectItem(buffer_field, "text_b64");
            textbuf_b64 = 1;
        }
        if (textbuf_field != NULL && !cJSON_IsNull(textbuf_field)) {
            char* buffer_str = cJSON_GetStringValue(textbuf_field);
            size_t buffer_str_len = strlen(buffer_str);
            if (textbuf_b64) {
                unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
                int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
                if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
                    // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
                    // because this will always be returned when checking size
                    ESP_LOGE(LOG_TAG, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in textbuf");
                    return ESP_FAIL;
                } else {
                    b64_len = 0;
                    pl_buffers[i].textBuffer = calloc(1, text_buffer_size);
                    result = mbedtls_base64_decode(pl_buffers[i].textBuffer, text_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
                    if (result != 0) {
                        ESP_LOGE(LOG_TAG, "mbedtls_base64_decode() failed for textbuf");
                        return ESP_FAIL;
                    }
                }
            } else {
                pl_buffers[i].textBuffer = calloc(1, text_buffer_size);
                memcpy(pl_buffers[i].textBuffer, buffer_str, MIN(buffer_str_len, text_buffer_size));
            }
        }
        
        cJSON* unitbuf_field = cJSON_GetObjectItem(buffer_field, "unit");
        uint8_t unitbuf_b64 = 0;
        if (unitbuf_field == NULL || cJSON_IsNull(unitbuf_field)) {
            unitbuf_field = cJSON_GetObjectItem(buffer_field, "unit_b64");
            unitbuf_b64 = 1;
        }
        if (unitbuf_field != NULL && !cJSON_IsNull(unitbuf_field)) {
            char* buffer_str = cJSON_GetStringValue(unitbuf_field);
            size_t buffer_str_len = strlen(buffer_str);
            if (unitbuf_b64) {
                unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
                int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
                if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
                    // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
                    // because this will always be returned when checking size
                    ESP_LOGE(LOG_TAG, "MBEDTLS_ERR_BASE64_INVALID_CHARACTER in unitbuf");
                    return ESP_FAIL;
                } else {
                    b64_len = 0;
                    pl_buffers[i].unitBuffer = calloc(1, unit_buffer_size);
                    result = mbedtls_base64_decode(pl_buffers[i].unitBuffer, unit_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
                    if (result != 0) {
                        ESP_LOGE(LOG_TAG, "mbedtls_base64_decode() failed for unitbuf");
                        return ESP_FAIL;
                    }
                }
            } else {
                pl_buffers[i].unitBuffer = calloc(1, unit_buffer_size);
                memcpy(pl_buffers[i].unitBuffer, buffer_str, MIN(buffer_str_len, unit_buffer_size));
            }
        }
    }

    // Save JSON to SPIFFS file if desired and possible
    if (pl_save_to_file && playlistFileValid) {
        char file_path[21]; // "/spiffs/" + 8.3 filename + null
        snprintf(file_path, 21, "/spiffs/%s", playlistFile);
        ESP_LOGI(LOG_TAG, "Writing file: %s", file_path);
        FILE* file = fopen(file_path, "w");
        if (file == NULL) {
            ESP_LOGE(LOG_TAG, "Failed to open file");
            return ESP_FAIL;
        }
        char *json_str = cJSON_Print(json);
        fprintf(file, json_str);
        fclose(file);
        cJSON_free(json_str);
    }

    return ESP_OK;
}