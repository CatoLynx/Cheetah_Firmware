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

// Dynamic array holding the current list of groups and buffers
static pl_buffer_group_t* pl_groups = NULL;
static uint16_t pl_num_groups = 0;
static uint16_t pl_cur_group = 0;
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
static portMUX_TYPE* pixel_buffer_lock = NULL;
static uint8_t* text_buffer;
static size_t text_buffer_size = 0;
static portMUX_TYPE* text_buffer_lock = NULL;
static uint8_t* unit_buffer;
static size_t unit_buffer_size = 0;
static portMUX_TYPE* unit_buffer_lock = NULL;

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

void playlist_init(nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock, uint8_t* textBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* unitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock) {
    ESP_LOGI(LOG_TAG, "Initializing playlist");
    pl_nvs_handle = *nvsHandle;
    pixel_buffer = pixBuf;
    pixel_buffer_size = pixBufSize;
    pixel_buffer_lock = pixBufLock;
    text_buffer = textBuf;
    text_buffer_size = textBufSize;
    text_buffer_lock = textBufLock;
    unit_buffer = unitBuf;
    unit_buffer_size = unitBufSize;
    unit_buffer_lock = unitBufLock;

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

    if (pollInterval != 0 && ((pollUrlValid && pollTokenValid) || playlistFileValid)) {
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

void playlist_update_buffers() {
    pl_buffers = pl_groups[pl_cur_group].entries;
    pl_num_buffers = pl_groups[pl_cur_group].numEntries;
}

void playlist_first_buffer() {
    pl_cur_buffer = 0;
}

void playlist_first_group() {
    if (pl_mode == PL_SEQUENTIAL) {
        pl_cur_group = 0;
    } else if (pl_mode == PL_RANDOM) {
        pl_cur_group = rand() % pl_num_groups;
    }
    playlist_update_buffers();
    playlist_first_buffer();
}

void playlist_next_group() {
    if (pl_mode == PL_SEQUENTIAL) {
        pl_cur_group++;
        if (pl_cur_group >= pl_num_groups) pl_cur_group = 0;
    } else if (pl_mode == PL_RANDOM) {
        pl_cur_group = rand() % pl_num_groups;
    }
    playlist_update_buffers();
    playlist_first_buffer();
}

void playlist_next_buffer() {
    pl_cur_buffer++;
    if (pl_cur_buffer >= pl_num_buffers) {
        pl_cur_buffer = 0;
        playlist_next_group();
    }
}

void playlist_task(void* arg) {
    while (1) {
        uint64_t now = esp_timer_get_time(); // Microseconds!

        // Switch buffer if necessary
        if (pl_num_groups > 0) {
            if (pl_restart_cycle == true || pl_last_switch == 0 || now - pl_last_switch >= pl_buffers[pl_cur_buffer].duration * 1000000) {
                if (!(pl_restart_cycle == true || pl_last_switch == 0)) playlist_next_buffer();
                if (pl_restart_cycle == true) {
                    ESP_LOGI(LOG_TAG, "Restarting cycle");
                    playlist_first_group();
                }
                pl_last_switch = now;
                pl_restart_cycle = false;

                if (pl_cur_buffer < pl_num_buffers) {
                    // If the playlist input is disabled in NVS with this flag,
                    // It'll keep running in the background, but not outputting anything
                    // This gets checked every loop cycle so that it takes immediate effect
                    uint8_t active = 0;
                    nvs_get_u8(pl_nvs_handle, "playlist_active", &active);
                    if (active) {
                        ESP_LOGD(LOG_TAG, "Switching to buffer %d", pl_cur_buffer);
                        if (pl_buffers[pl_cur_buffer].pixelBuffer != NULL) {
                            taskENTER_CRITICAL(pixel_buffer_lock);
                            memcpy(pixel_buffer, pl_buffers[pl_cur_buffer].pixelBuffer, pixel_buffer_size);
                            taskEXIT_CRITICAL(pixel_buffer_lock);
                        }
                        if (pl_buffers[pl_cur_buffer].textBuffer  != NULL) {
                            taskENTER_CRITICAL(text_buffer_lock);
                            memcpy(text_buffer,  pl_buffers[pl_cur_buffer].textBuffer,  text_buffer_size);
                            taskEXIT_CRITICAL(text_buffer_lock);
                        }
                        if (pl_buffers[pl_cur_buffer].unitBuffer  != NULL) {
                            taskENTER_CRITICAL(unit_buffer_lock);
                            memcpy(unit_buffer,  pl_buffers[pl_cur_buffer].unitBuffer,  unit_buffer_size);
                            taskEXIT_CRITICAL(unit_buffer_lock);
                        }

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
        ESP_LOGE(LOG_TAG, "Error");
    }
    cJSON_Delete(json);
}

esp_err_t playlist_process_json(cJSON* json) {
    /*
    Expected JSON schema (only non-null buffers will be updated):
    {
        "buffers":  [
            [
                {
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
                            "glitch_non_blank": true,
                            "glitch_blank": false,
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
                }
            ],
            [
                {
                    "buffer": {
                        "text_b64": "<Base64 encoded buffer>"
                    },
                    "duration": 1,
                    "brightness": 255,
                    "effect": null
                }
            ]],
        "playlistMode": "<random|sequential>",
        "restartCycle": false
    }

    On error:
    {
        "error": "Some error here"
    }
    */

    ESP_LOGD(LOG_TAG, "Processing JSON");
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
    uint16_t numGroups = cJSON_GetArraySize(buffers_arr);

    // Free individual buffers, buffer array and the group array
    for (uint16_t j = 0; j < pl_num_groups; j++) {
        for (uint16_t i = 0; i < pl_groups[j].numEntries; i++) {
            free(pl_groups[j].entries[i].pixelBuffer);
            free(pl_groups[j].entries[i].textBuffer);
            free(pl_groups[j].entries[i].unitBuffer);
        }
        free(pl_groups[j].entries);
    }
    free(pl_groups);

    // Allocate new groups
    ESP_LOGD(LOG_TAG, "Allocating %d groups", numGroups);
    pl_num_groups = numGroups;
    pl_groups = calloc(pl_num_groups, sizeof(pl_buffer_group_t));

    for (uint16_t j = 0; j < numGroups; j++) {
        cJSON* sub_arr = cJSON_GetArrayItem(buffers_arr, j);
        if (!cJSON_IsArray(sub_arr)) {
            ESP_LOGE(LOG_TAG, "'buffers[%u]' is not an array'", j);
            return ESP_FAIL;
        }
        uint16_t numBuffers = cJSON_GetArraySize(sub_arr);

        // Allocate new buffers
        ESP_LOGD(LOG_TAG, "Allocating %d buffers", numBuffers);
        pl_groups[j].entries = calloc(numBuffers, sizeof(pl_buffer_list_entry_t));
        pl_groups[j].numEntries = numBuffers;

        // Populate new buffers
        for (uint16_t i = 0; i < numBuffers; i++) {
            cJSON* item = cJSON_GetArrayItem(sub_arr, i);

            cJSON* duration_field = cJSON_GetObjectItem(item, "duration");
            pl_groups[j].entries[i].duration = cJSON_GetNumberValue(duration_field);

            cJSON* brightness_field = cJSON_GetObjectItem(item, "brightness");
            if (brightness_field != NULL && !cJSON_IsNull(brightness_field)) {
                pl_groups[j].entries[i].brightness = cJSON_GetNumberValue(brightness_field);
            } else  {
                pl_groups[j].entries[i].brightness = -1;
            }

            // For shader, transition, effect and bitmap generator fields,
            // the rule is that an explicit null entry in the JSON
            // clears the respective configuration while an omitted entry
            // keeps the previous configuration.

            cJSON* shader_field = cJSON_GetObjectItem(item, "shader");
            if (pl_groups[j].entries[i].shader != NULL) cJSON_Delete(pl_groups[j].entries[i].shader);
            pl_groups[j].entries[i].updateShader = (shader_field != NULL);
            pl_groups[j].entries[i].shader = cJSON_Duplicate(shader_field, true);

            cJSON* transition_field = cJSON_GetObjectItem(item, "transition");
            if (pl_groups[j].entries[i].transition != NULL) cJSON_Delete(pl_groups[j].entries[i].transition);
            pl_groups[j].entries[i].updateTransition = (transition_field != NULL);
            pl_groups[j].entries[i].transition = cJSON_Duplicate(transition_field, true);

            cJSON* effect_field = cJSON_GetObjectItem(item, "effect");
            if (pl_groups[j].entries[i].effect != NULL) cJSON_Delete(pl_groups[j].entries[i].effect);
            pl_groups[j].entries[i].updateEffect = (effect_field != NULL);
            pl_groups[j].entries[i].effect = cJSON_Duplicate(effect_field, true);

            cJSON* bitmap_generator_field = cJSON_GetObjectItem(item, "bitmap_generator");
            if (pl_groups[j].entries[i].bitmapGenerator != NULL) cJSON_Delete(pl_groups[j].entries[i].bitmapGenerator);
            pl_groups[j].entries[i].updateBitmapGenerator = (bitmap_generator_field != NULL);
            pl_groups[j].entries[i].bitmapGenerator = cJSON_Duplicate(bitmap_generator_field, true);

            cJSON* buffer_field = cJSON_GetObjectItem(item, "buffer");

            cJSON* pixbuf_field = cJSON_GetObjectItem(buffer_field, "pixel");
            uint8_t pixbuf_b64 = 0;
            if (pixbuf_field == NULL || cJSON_IsNull(pixbuf_field)) {
                pixbuf_field = cJSON_GetObjectItem(buffer_field, "pixel_b64");
                pixbuf_b64 = 1;
            }
            if (pixbuf_field != NULL && !cJSON_IsNull(pixbuf_field)) {
                char* buffer_str = cJSON_GetStringValue(pixbuf_field);
                pl_groups[j].entries[i].pixelBuffer = heap_caps_calloc(1, pixel_buffer_size, MALLOC_CAP_SPIRAM);
                esp_err_t ret = buffer_from_string(buffer_str, pixbuf_b64, pl_groups[j].entries[i].pixelBuffer, pixel_buffer_size, LOG_TAG);
                if (ret != ESP_OK) free(pl_groups[j].entries[i].pixelBuffer);
            }
            
            cJSON* textbuf_field = cJSON_GetObjectItem(buffer_field, "text");
            uint8_t textbuf_b64 = 0;
            if (textbuf_field == NULL || cJSON_IsNull(textbuf_field)) {
                textbuf_field = cJSON_GetObjectItem(buffer_field, "text_b64");
                textbuf_b64 = 1;
            }
            if (textbuf_field != NULL && !cJSON_IsNull(textbuf_field)) {
                char* buffer_str = cJSON_GetStringValue(textbuf_field);
                pl_groups[j].entries[i].textBuffer = calloc(1, text_buffer_size);
                esp_err_t ret = buffer_from_string(buffer_str, textbuf_b64, pl_groups[j].entries[i].textBuffer, text_buffer_size, LOG_TAG);
                if (ret != ESP_OK) free(pl_groups[j].entries[i].textBuffer);
            }
            
            cJSON* unitbuf_field = cJSON_GetObjectItem(buffer_field, "unit");
            uint8_t unitbuf_b64 = 0;
            if (unitbuf_field == NULL || cJSON_IsNull(unitbuf_field)) {
                unitbuf_field = cJSON_GetObjectItem(buffer_field, "unit_b64");
                unitbuf_b64 = 1;
            }
            if (unitbuf_field != NULL && !cJSON_IsNull(unitbuf_field)) {
                char* buffer_str = cJSON_GetStringValue(unitbuf_field);
                pl_groups[j].entries[i].unitBuffer = calloc(1, unit_buffer_size);
                esp_err_t ret = buffer_from_string(buffer_str, unitbuf_b64, pl_groups[j].entries[i].unitBuffer, unit_buffer_size, LOG_TAG);
                if (ret != ESP_OK) free(pl_groups[j].entries[i].unitBuffer);
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

    if (pl_cur_group >= pl_num_groups) playlist_first_group(); // In case pl_num_groups got smaller
    playlist_update_buffers();
    return ESP_OK;
}