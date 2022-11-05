#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "cJSON.h"

#include "browser_config.h"
#include "util_httpd.h"
#include "util_generic.h"

#define LOG_TAG "BROWSER-CONFIG"

static httpd_handle_t* config_server;

// Embedded files - refer to CMakeLists.txt
extern const uint8_t browser_config_html_start[] asm("_binary_browser_config_html_start");
extern const uint8_t browser_config_html_end[]   asm("_binary_browser_config_html_end");


// List of config options to present
nvs_handle_t config_nvs_handle;
config_entry_t config_entries[] = {
    {.key = "sta_ssid", .dataType = STR},
    {.key = "sta_pass", .dataType = STR},
    {.key = "sta_retries", .dataType = U8},
    {.key = "tg_bot_token", .dataType = STR},
};


static esp_err_t config_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)browser_config_html_start, browser_config_html_end - browser_config_html_start);
    return ESP_OK;
}

static esp_err_t config_get_fields_handler(httpd_req_t *req) {
    /*
    Example response:

    {"fields": [{"name": "sta_ssid", "type": 12, "value": "ABCDEFGHIJ"}, {"name": "sta_pass", "type": 12, "value": "1234567890"}]}
    */

    cJSON* json = cJSON_CreateObject();
    cJSON* fields_arr = cJSON_AddArrayToObject(json, "fields");

    uint16_t num_fields = sizeof(config_entries) / sizeof(config_entries[0]);

    for (uint16_t i = 0; i < num_fields; i++) {
        cJSON* entry = cJSON_CreateObject();
        cJSON_AddStringToObject(entry, "name", config_entries[i].key);
        cJSON_AddNumberToObject(entry, "type", config_entries[i].dataType);

        if (config_entries[i].dataType == BLOB) {
            // Not yet implemented
        } else if (config_entries[i].dataType == STR) {
            // Query string length
            size_t valueLength;
            esp_err_t ret = nvs_get_str(config_nvs_handle, config_entries[i].key, NULL, &valueLength);
            if (ret == ESP_ERR_NVS_NOT_FOUND) {
                cJSON_AddStringToObject(entry, "value", "");
            } else {
                ESP_ERROR_CHECK(ret);
                // Allocate buffer for value
                char* value = malloc(valueLength);
                // Read value
                ESP_ERROR_CHECK(nvs_get_str(config_nvs_handle, config_entries[i].key, value, &valueLength));
                cJSON_AddStringToObject(entry, "value", value);
                // Free allocated memory
                free(value);
            }
        } else {
            // Numerical value
            switch(config_entries[i].dataType) {
                case I8: {
                    int8_t value;
                    esp_err_t ret = nvs_get_i8(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case U8: {
                    uint8_t value;
                    esp_err_t ret = nvs_get_u8(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case I16: {
                    int16_t value;
                    esp_err_t ret = nvs_get_i16(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case U16: {
                    uint16_t value;
                    esp_err_t ret = nvs_get_u16(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case I32: {
                    int32_t value;
                    esp_err_t ret = nvs_get_i32(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case U32: {
                    uint32_t value;
                    esp_err_t ret = nvs_get_u32(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case I64: {
                    int64_t value;
                    esp_err_t ret = nvs_get_i64(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                case U64: {
                    uint64_t value;
                    esp_err_t ret = nvs_get_u64(config_nvs_handle, config_entries[i].key, &value);
                    if (ret == ESP_ERR_NVS_NOT_FOUND) {
                        cJSON_AddNumberToObject(entry, "value", 0);
                    } else {
                        ESP_ERROR_CHECK(ret);
                        cJSON_AddNumberToObject(entry, "value", value);
                    }
                    break;
                }
                default: break;
            }
        }
        cJSON_AddItemToArray(fields_arr, entry);
    }

    char *resp = cJSON_Print(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t config_post_update_handler(httpd_req_t *req) {
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    }

    cJSON* fields_arr = cJSON_GetObjectItem(json, "fields");
    if (!cJSON_IsArray(fields_arr)) {
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    }

    cJSON* entry = NULL;
    cJSON_ArrayForEach(entry, fields_arr) {
        if (!cJSON_IsObject(entry)) {
            cJSON_Delete(json);
            return abortRequest(req, "500 Internal Server Error");
        }

        cJSON* field_name = cJSON_GetObjectItem(entry, "name");
        if (!cJSON_IsString(field_name)) {
            cJSON_Delete(json);
            return abortRequest(req, "500 Internal Server Error");
        }
        char* fieldName = cJSON_GetStringValue(field_name);

        cJSON* field_type = cJSON_GetObjectItem(entry, "type");
        if (!cJSON_IsNumber(field_type)) {
            cJSON_Delete(json);
            return abortRequest(req, "500 Internal Server Error");
        }
        config_data_type_t fieldType = (config_data_type_t)(int)cJSON_GetNumberValue(field_type);

        cJSON* field_value = cJSON_GetObjectItem(entry, "value");
        switch(fieldType) {
            case I8:
            case U8:
            case I16:
            case U16:
            case I32:
            case U32:
            case I64:
            case U64: {
                if (!cJSON_IsNumber(field_value)) {
                    cJSON_Delete(json);
                    return abortRequest(req, "500 Internal Server Error");
                }
                break;
            }
            case STR: {
                if (!cJSON_IsString(field_value)) {
                    cJSON_Delete(json);
                    return abortRequest(req, "500 Internal Server Error");
                }
                break;
            }
            default: {
                cJSON_Delete(json);
                return abortRequest(req, "500 Internal Server Error");
            }
        }

        switch(fieldType) {
            case I8: {
                int8_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_i8(config_nvs_handle, fieldName, value));
                break;
            }
            case U8: {
                uint8_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_u8(config_nvs_handle, fieldName, value));
                break;
            }
            case I16: {
                int16_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_i16(config_nvs_handle, fieldName, value));
                break;
            }
            case U16: {
                uint16_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_u16(config_nvs_handle, fieldName, value));
                break;
            }
            case I32: {
                int32_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_i32(config_nvs_handle, fieldName, value));
                break;
            }
            case U32: {
                uint32_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_u32(config_nvs_handle, fieldName, value));
                break;
            }
            case I64: {
                int64_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_i64(config_nvs_handle, fieldName, value));
                break;
            }
            case U64: {
                uint64_t value = cJSON_GetNumberValue(field_value);
                ESP_ERROR_CHECK(nvs_set_u64(config_nvs_handle, fieldName, value));
                break;
            }
            case STR: {
                char* value = cJSON_GetStringValue(field_value);
                ESP_ERROR_CHECK(nvs_set_str(config_nvs_handle, fieldName, value));
                break;
            }
            default: {
                cJSON_Delete(json);
                return abortRequest(req, "500 Internal Server Error");
            }
        }
    }

    nvs_commit(config_nvs_handle);
    cJSON_Delete(json);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t config_get = {
    .uri       = "/config",
    .method    = HTTP_GET,
    .handler   = config_get_handler
};

static const httpd_uri_t config_get_fields = {
    .uri       = "/config/fields.json",
    .method    = HTTP_GET,
    .handler   = config_get_fields_handler
};

static const httpd_uri_t config_post_update = {
    .uri       = "/config/update",
    .method    = HTTP_POST,
    .handler   = config_post_update_handler
};

void browser_config_init(httpd_handle_t* server, nvs_handle_t* nvsHandle) {
    config_nvs_handle = *nvsHandle;
    ESP_LOGI(LOG_TAG, "Init");
    ESP_LOGI(LOG_TAG, "Registering URI handlers");
    httpd_register_uri_handler(*server, &config_get);
    httpd_register_uri_handler(*server, &config_get_fields);
    httpd_register_uri_handler(*server, &config_post_update);
    config_server = server;
}

void browser_config_deinit(void) {
    ESP_LOGI(LOG_TAG, "De-Init");
    ESP_LOGI(LOG_TAG, "Unregistering URI handlers");
    httpd_unregister_uri_handler(*config_server, config_get.uri, config_get.method);
    httpd_unregister_uri_handler(*config_server, config_get_fields.uri, config_get_fields.method);
    httpd_unregister_uri_handler(*config_server, config_post_update.uri, config_post_update.method);
}