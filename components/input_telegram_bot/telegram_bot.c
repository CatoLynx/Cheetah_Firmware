#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "esp_http_client.h"

#include "telegram_bot.h"
#include "util_nvs.h"
#include "util_generic.h"


#define LOG_TAG "TGBot"

static TaskHandle_t telegram_bot_task_handle;
nvs_handle_t tg_bot_nvs_handle;
char* apiToken = NULL;
uint8_t apiTokenInited = 0;
uint32_t last_update_id = 0;
uint8_t err_status = 0;

uint8_t* output_buffer;
size_t output_buffer_size = 0;


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

            esp_err_t ret = telegram_bot_process_response((telegram_api_endpoint_t)(evt->user_data), json);
            if (ret != ESP_OK) {
                memset(output_buffer, 0x00, output_buffer_size);
                sprintf((char*)output_buffer, "TELEGRAM API FAIL %u", err_status);
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
    if (apiToken != NULL) apiTokenInited = 1;
    
    xTaskCreate(telegram_bot_task, "telegram_bot", 4096, NULL, 5, &telegram_bot_task_handle);
}

void telegram_bot_deinit() {
    if (apiTokenInited) {
        // Free allocated memory
        free(apiToken);
    }
}

void telegram_bot_task(void* arg) {
    while (1) {
        //strncpy((char*)output_buffer, "GET_UPDATES", output_buffer_size);
        telegram_bot_send_request(TG_GET_UPDATES);
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
            post_data = cJSON_Print(json);

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
        sprintf((char*)output_buffer, "GET FAILED %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

esp_err_t telegram_bot_process_response(telegram_api_endpoint_t endpoint, cJSON* json) {
    cJSON* field_ok = cJSON_GetObjectItem(json, "ok");
    if (field_ok == NULL) return ESP_FAIL;
    if (!cJSON_IsTrue(field_ok)) return ESP_FAIL;

    cJSON* result = cJSON_GetObjectItem(json, "result");
    if (result == NULL) return ESP_FAIL;

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
                sprintf((char*)output_buffer, "USERNAME=%s", username);
            }
            break;
        }
        
        case TG_GET_UPDATES: {
            if (!cJSON_IsArray(result)) {
                err_status = 3;
                return ESP_FAIL;
            }
            uint16_t arraySize = cJSON_GetArraySize(result);

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

                char* text = cJSON_GetStringValue(field_text);
                char* filteredText = malloc(strlen(text) + 1);
                memset(filteredText, 0x00, strlen(text) + 1);

                #if defined(CONFIG_TG_BOT_FORCE_UPPERCASE)
                str_toUpper(text);
                #endif

                #if defined(CONFIG_TG_BOT_CHARSET_METHOD_ALLOWED_CHARS_STR)
                str_filterAllowed(filteredText, text, CONFIG_TG_BOT_ALLOWED_CHARACTERS_STR);
                #elif defined(CONFIG_TG_BOT_CHARSET_METHOD_DISALLOWED_CHARS_STR)
                str_filterDisallowed(filteredText, text, CONFIG_TG_BOT_DISALLOWED_CHARACTERS_STR);
                #elif defined(CONFIG_TG_BOT_CHARSET_METHOD_ALLOWED_CHARS_RANGE)
                str_filterRangeAllowed(filteredText, text, CONFIG_TG_BOT_ALLOWED_CHARACTERS_RANGE_MIN, CONFIG_TG_BOT_ALLOWED_CHARACTERS_RANGE_MAX);
                #elif defined(CONFIG_TG_BOT_CHARSET_METHOD_DISALLOWED_CHARS_RANGE)
                str_filterRangeDisallowed(filteredText, text, CONFIG_TG_BOT_DISALLOWED_CHARACTERS_RANGE_MIN, CONFIG_TG_BOT_DISALLOWED_CHARACTERS_RANGE_MAX);
                #endif

                strncpy((char*)output_buffer, filteredText, output_buffer_size);
                free(filteredText);

                telegram_bot_send_request(TG_SEND_MESSAGE, chat_id, "Done!");
            }
            break;
        }

        case TG_SEND_MESSAGE: {
            break;
        }
    }
    return ESP_OK;
}