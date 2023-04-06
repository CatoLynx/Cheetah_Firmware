#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "esp_http_client.h"

#include "telegram_bot.h"
#include "macros.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_nvs.h"


#if defined(CONFIG_DISPLAY_TYPE_PIXEL) || defined(CONFIG_DISPLAY_TYPE_CHARACTER)

#define LOG_TAG "TGBot"

static TaskHandle_t telegram_bot_task_handle;
nvs_handle_t tg_bot_nvs_handle;
char* apiToken = NULL;
uint8_t apiTokenInited = 0;
uint32_t last_update_id = 0;
uint8_t err_status = 0;
char* err_desc = NULL;

uint8_t* output_buffer;
size_t output_buffer_size = 0;

extern uint8_t wifi_gotIP;


esp_err_t telegram_bot_http_event_handler(esp_http_client_event_t *evt) {
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
                        err_status = 18;
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
                err_status = 19;
                return ESP_FAIL;
            }
            resp_len = 0;

            esp_err_t ret = telegram_bot_process_response((telegram_api_endpoint_t)(evt->user_data), json);
            if (ret != ESP_OK) {
                memset(output_buffer, 0x00, output_buffer_size);
                if (err_desc == NULL) {
                    ESP_LOGE(LOG_TAG,  "Error status %u", err_status);
                    // sprintf((char*)output_buffer, "TELEGRAM API FAIL %u", err_status);
                } else {
                    ESP_LOGE(LOG_TAG,  "Error: %s", err_desc);
                    // strncpy((char*)output_buffer, err_desc, output_buffer_size);
                    err_desc = NULL;
                }
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

void telegram_bot_init(nvs_handle_t* nvsHandle, uint8_t* outBuf, size_t bufSize) {
    tg_bot_nvs_handle = *nvsHandle;
    output_buffer = outBuf;
    output_buffer_size = bufSize;

    if (apiTokenInited) {
        // Free allocated memory
        free(apiToken);
    }
    apiToken = get_string_from_nvs(nvsHandle, "tg_bot_token");
    if (apiToken != NULL) {
        apiTokenInited = 1;
        if (strlen(apiToken) != 0) {
            xTaskCreatePinnedToCore(telegram_bot_task, "telegram_bot", 4096, NULL, 5, &telegram_bot_task_handle, 0);
        }
    }
}

void telegram_bot_deinit() {
    if (apiTokenInited) {
        // Free allocated memory
        free(apiToken);
    }
}

void telegram_bot_task(void* arg) {
    while (1) {
        if (wifi_gotIP) telegram_bot_send_request(TG_GET_UPDATES);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void telegram_bot_send_request(telegram_api_endpoint_t endpoint, ...) {
    va_list valist;
    char url[200];
    esp_http_client_config_t config = {
        .event_handler = telegram_bot_http_event_handler,
        .disable_auto_redirect = false,
        .user_data = (void*)endpoint,
    };
    cJSON* json;
    char* post_data;

    switch(endpoint) {
        case TG_GET_ME: {
            sprintf(url, "https://api.telegram.org/bot%s/getMe", apiToken);
            config.url = url;
            break;
        }

        case TG_GET_UPDATES: {
            sprintf(url, "https://api.telegram.org/bot%s/getUpdates?allowed_updates=[\"message\"]&limit=%u&offset=%u&timeout=%u", apiToken, CONFIG_TG_BOT_MESSAGE_LIMIT, last_update_id + 1, CONFIG_TG_BOT_POLLING_TIMEOUT);
            config.url = url;
            config.timeout_ms = (CONFIG_TG_BOT_POLLING_TIMEOUT + 2) * 1000;
            break;
        }

        case TG_SEND_MESSAGE: {
            sprintf(url, "https://api.telegram.org/bot%s/sendMessage", apiToken);
            config.url = url;
            config.method = HTTP_METHOD_POST;
            break;
        }
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);

    switch(endpoint) {
        case TG_SEND_MESSAGE: {
            va_start(valist, endpoint);
            uint32_t chat_id = va_arg(valist, uint32_t);
            char* text = va_arg(valist, char*);

            json = cJSON_CreateObject();
            cJSON_AddNumberToObject(json, "chat_id", chat_id);
            cJSON_AddStringToObject(json, "text", text);
            cJSON_AddStringToObject(json, "parse_mode", "MarkdownV2");
            post_data = cJSON_Print(json);
            ESP_LOGV(LOG_TAG, "POST Data: %s", post_data);

            esp_http_client_set_header(client, "Content-Type", "application/json");
            esp_http_client_set_post_field(client, post_data, strlen(post_data));
            break;
        }

        default: break;
    }

    esp_err_t err = esp_http_client_perform(client);

    switch(endpoint) {
        case TG_SEND_MESSAGE: {
            cJSON_Delete(json);
            cJSON_free(post_data);
            break;
        }

        default: break;
    }

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

esp_err_t telegram_bot_process_response(telegram_api_endpoint_t endpoint, cJSON* json) {
    ESP_LOGD(LOG_TAG, "Processing response");
    cJSON* field_ok = cJSON_GetObjectItem(json, "ok");
    if (field_ok == NULL) {
        err_status = 16;
        return ESP_FAIL;
    }
    if (!cJSON_IsTrue(field_ok)) {
        cJSON* field_error_code = cJSON_GetObjectItem(json, "error_code");
        uint16_t error_code = cJSON_GetNumberValue(field_error_code);
        err_status = error_code;
        cJSON* field_error_desc = cJSON_GetObjectItem(json, "description");
        err_desc = cJSON_GetStringValue(field_error_desc);
        ESP_LOGE(LOG_TAG, "Telegram API Error %d: %s", error_code, err_desc);
        return ESP_FAIL;
    }

    cJSON* result = cJSON_GetObjectItem(json, "result");
    if (result == NULL) {
        err_status = 20;
        return ESP_FAIL;
    }

    ESP_LOGD(LOG_TAG, "Endpoint: %d", (int)endpoint);
    switch(endpoint) {
        case TG_GET_ME: {
            if (!cJSON_IsObject(result)) {
                err_status = 1;
                return ESP_FAIL;
            }
            cJSON* field_username = cJSON_GetObjectItem(result, "username");
            if (field_username == NULL) {
                err_status = 2;
                return ESP_FAIL;
            }
            if (cJSON_IsString(field_username)) {
                char* username = cJSON_GetStringValue(field_username);
                str_toUpper(username);
                memset(output_buffer, 0x00, output_buffer_size);
                ESP_LOGI(LOG_TAG, "Telegram Username: @%s", username);
                // sprintf((char*)output_buffer, "USERNAME=%s", username);
            }
            break;
        }
        
        case TG_GET_UPDATES: {
            if (!cJSON_IsArray(result)) {
                err_status = 3;
                return ESP_FAIL;
            }
            uint16_t arraySize = cJSON_GetArraySize(result);
            ESP_LOGD(LOG_TAG, "Got %u messages", arraySize);

            for (uint16_t i = 0; i < arraySize; i++) {
                cJSON* item = cJSON_GetArrayItem(result, i);
                if (item == NULL) {
                    err_status = 4;
                    return ESP_FAIL;
                }
                if (!cJSON_IsObject(item)) {
                    err_status = 5;
                    return ESP_FAIL;
                }

                cJSON* field_update_id = cJSON_GetObjectItem(item, "update_id");
                if (field_update_id == NULL) {
                    err_status = 6;
                    return ESP_FAIL;
                }
                if (!cJSON_IsNumber(field_update_id)) {
                    err_status = 7;
                    return ESP_FAIL;
                }
                last_update_id = cJSON_GetNumberValue(field_update_id);

                cJSON* message = cJSON_GetObjectItem(item, "message");
                if (message == NULL) {
                    err_status = 8;
                    return ESP_FAIL;
                }
                if (!cJSON_IsObject(message)) {
                    err_status = 9;
                    return ESP_FAIL;
                }

                cJSON* chat = cJSON_GetObjectItem(message, "chat");
                if (chat == NULL) {
                    err_status = 10;
                    return ESP_FAIL;
                }
                if (!cJSON_IsObject(chat)) {
                    err_status = 11;
                    return ESP_FAIL;
                }

                cJSON* field_chat_id = cJSON_GetObjectItem(chat, "id");
                if (field_chat_id == NULL) {
                    err_status = 12;
                    return ESP_FAIL;
                }
                if (!cJSON_IsNumber(field_chat_id)) {
                    err_status = 13;
                    return ESP_FAIL;
                }
                uint32_t chat_id = cJSON_GetNumberValue(field_chat_id);

                cJSON* field_text = cJSON_GetObjectItem(message, "text");
                if (field_text == NULL) {
                    err_status = 14;
                    return ESP_FAIL;
                }
                if (!cJSON_IsString(field_text)) {
                    err_status = 15;
                    return ESP_FAIL;
                }

                char* text_utf8 = cJSON_GetStringValue(field_text);
                

                char* filteredText_utf8 = malloc(strlen(text_utf8) + 1);
                memset(filteredText_utf8, 0x00, strlen(text_utf8) + 1);

                ESP_LOGD(LOG_TAG, "Filtering message text");

                #if defined(CONFIG_TG_BOT_FORCE_UPPERCASE)
                str_toUpper(text_utf8); // TODO: Handle UTF-8 correctly
                #endif

                #if defined(CONFIG_TG_BOT_CHARSET_METHOD_ALLOWED_CHARS_STR)
                str_filterAllowed(filteredText_utf8, text_utf8, CONFIG_TG_BOT_ALLOWED_CHARACTERS_STR, true);
                #elif defined(CONFIG_TG_BOT_CHARSET_METHOD_DISALLOWED_CHARS_STR)
                str_filterDisallowed(filteredText_utf8, text_utf8, CONFIG_TG_BOT_DISALLOWED_CHARACTERS_STR, true);
                #elif defined(CONFIG_TG_BOT_CHARSET_METHOD_ALLOWED_CHARS_RANGE)
                str_filterRangeAllowed(filteredText_utf8, text_utf8, CONFIG_TG_BOT_ALLOWED_CHARACTERS_RANGE_MIN, CONFIG_TG_BOT_ALLOWED_CHARACTERS_RANGE_MAX, true);
                #elif defined(CONFIG_TG_BOT_CHARSET_METHOD_DISALLOWED_CHARS_RANGE)
                str_filterRangeDisallowed(filteredText_utf8, text_utf8, CONFIG_TG_BOT_DISALLOWED_CHARACTERS_RANGE_MIN, CONFIG_TG_BOT_DISALLOWED_CHARACTERS_RANGE_MAX, true);
                #endif

                char* filteredText_iso88591 = malloc(strlen(filteredText_utf8) + 1); // Text can only be same length or less in ISO-8859-1
                memset(filteredText_iso88591, 0x00, strlen(filteredText_utf8) + 1);
                ESP_LOGD(LOG_TAG, "Converting message text from UTF-8 to ISO-8859-1");
                buffer_utf8_to_iso88591(filteredText_iso88591, filteredText_utf8);
                ESP_LOGD(LOG_TAG, "Result: %s", filteredText_iso88591);

                ESP_LOGD(LOG_TAG, "Converting message for display");
                memset(output_buffer, 0x00, output_buffer_size);
                str_convertLineBreaks((char*)output_buffer, filteredText_iso88591, DISPLAY_FRAME_HEIGHT, DISPLAY_FRAME_WIDTH);
                free(filteredText_utf8);
                free(filteredText_iso88591);

                ESP_LOGD(LOG_TAG, "Converting output buffer for Telegram reply");
                ESP_LOGD(LOG_TAG, "Free Heap: %u", esp_get_free_heap_size());
                char* formattedOutput = malloc(output_buffer_size + DISPLAY_FRAME_HEIGHT); // + DISPLAY_FRAME_HEIGHT to get one newline character per line
                ESP_LOGV(LOG_TAG, "formattedOutput = %p", formattedOutput);
                memset(formattedOutput, 0x00, output_buffer_size + DISPLAY_FRAME_HEIGHT);
                ESP_LOGV(LOG_TAG, "str_insertLineBreaks");
                str_insertLineBreaks(formattedOutput, (char*)output_buffer, DISPLAY_FRAME_WIDTH, output_buffer_size);

                ESP_LOGD(LOG_TAG, "Sending reply");
                char* tgText;
                asprintf(&tgText, "Current display text:\n\n`%s`", formattedOutput);

                char* tgText_utf8 = malloc(strlen(tgText) * 2 + 1); // Text can be at most twice as long in UTF-8
                memset(tgText_utf8, 0x00, strlen(tgText) * 2 + 1);
                ESP_LOGD(LOG_TAG, "Converting reply text from ISO-8859-1 to UTF-8");
                buffer_iso88591_to_utf8(tgText_utf8, tgText);
                ESP_LOGD(LOG_TAG, "Result: %s", tgText_utf8);
                telegram_bot_send_request(TG_SEND_MESSAGE, chat_id, tgText_utf8);
                free(tgText_utf8);
                free(formattedOutput);
                free(tgText);
                ESP_LOGD(LOG_TAG, "Message processing finished");
            }
            break;
        }

        case TG_SEND_MESSAGE: {
            break;
        }
    }
    return ESP_OK;
}

#endif