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

    uint16_t num_fields = sizeof(config_entries) / sizeof(config_entries[0]);

    size_t resp_len = 15; // {"fields": []} + NUL
    size_t valueLen = 0;
    for (uint16_t i = 0; i < num_fields; i++) {
        resp_len += 37; // {"name": "", "type": XX, "value": }, 
        resp_len += strlen(config_entries[i].key);
        if (config_entries[i].dataType == STR) {
            ESP_ERROR_CHECK(nvs_get_str(config_nvs_handle, config_entries[i].key, NULL, &valueLen));
            resp_len += valueLen + 2; // plus ""
        } else if (config_entries[i].dataType == BLOB) {
            // Not yet implemented
            resp_len += 4; // Just return null
        } else {
            // Numerical values, just allocate conservatively for the highest possible value
            switch(config_entries[i].dataType) {
                case I8: {
                    resp_len += 4; // -128
                    break;
                }
                case U8: {
                    resp_len += 3; // 255
                    break;
                }
                case I16: {
                    resp_len += 6; // -32768
                    break;
                }
                case U16: {
                    resp_len += 5; // 65535
                    break;
                }
                case I32: {
                    resp_len += 11; // -2147483648
                    break;
                }
                case U32: {
                    resp_len += 10; // 4294967295
                    break;
                }
                case I64: {
                    resp_len += 20; // -9223372036854775808
                    break;
                }
                case U64: {
                    resp_len += 20; // 18446744073709551615
                    break;
                }
                default: break;
            }
        }
    }

    char* resp = malloc(resp_len);
    memset(resp, 0x00, resp_len);
    uint32_t resp_ptr = 0;

    strcpy(resp + resp_ptr, "{\"fields\": [");
    resp_ptr += 12;

    for (uint16_t i = 0; i < num_fields; i++) {
        strcpy(resp + resp_ptr, "{\"name\": \"");
        resp_ptr += 10;

        strcpy(resp + resp_ptr, config_entries[i].key);
        resp_ptr += strlen(config_entries[i].key);

        strcpy(resp + resp_ptr, "\", \"type\": ");
        resp_ptr += 11;

        sprintf(resp + resp_ptr, "%u", config_entries[i].dataType); // Writes NUL but we'll overwrite it in the next strcpy
        resp_ptr += 2; // They're all >= 10 and <= 99, so two characters

        strcpy(resp + resp_ptr, ", \"value\": ");
        resp_ptr += 11;

        if (config_entries[i].dataType == STR) {
            strcpy(resp + resp_ptr, "\"");
            resp_ptr += 1;

            valueLen = resp_len - resp_ptr; // length pointer for nvs_get_str must be initialized with target buffer size
            ESP_ERROR_CHECK(nvs_get_str(config_nvs_handle, config_entries[i].key, resp + resp_ptr, &valueLen)); // Writes NUL but we'll overwrite it in the next strcpy
            if (valueLen > 0) resp_ptr += valueLen - 1; // -1 to ignore NUL
            
            strcpy(resp + resp_ptr, "\"");
            resp_ptr += 1;
        } else if (config_entries[i].dataType == BLOB) {
            // Not yet implemented
            strcpy(resp + resp_ptr, "null");
            resp_ptr += 4;
        } else {
            // Numerical values
            switch(config_entries[i].dataType) {
                case I8: {
                    int8_t res;
                    ESP_ERROR_CHECK(nvs_get_i8(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%d", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += int_num_digits(res, 1);
                    break;
                }
                case U8: {
                    uint8_t res;
                    ESP_ERROR_CHECK(nvs_get_u8(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%u", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += uint_num_digits(res);
                    break;
                }
                case I16: {
                    int16_t res;
                    ESP_ERROR_CHECK(nvs_get_i16(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%d", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += int_num_digits(res, 1);
                    break;
                }
                case U16: {
                    uint16_t res;
                    ESP_ERROR_CHECK(nvs_get_u16(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%u", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += uint_num_digits(res);
                    break;
                }
                case I32: {
                    int32_t res;
                    ESP_ERROR_CHECK(nvs_get_i32(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%d", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += int_num_digits(res, 1);
                    break;
                }
                case U32: {
                    uint32_t res;
                    ESP_ERROR_CHECK(nvs_get_u32(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%u", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += uint_num_digits(res);
                    break;
                }
                case I64: {
                    int64_t res;
                    ESP_ERROR_CHECK(nvs_get_i64(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%lld", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += int_num_digits(res, 1);
                    break;
                }
                case U64: {
                    uint64_t res;
                    ESP_ERROR_CHECK(nvs_get_u64(config_nvs_handle, config_entries[i].key, &res));
                    sprintf(resp + resp_ptr, "%llu", res); // Writes NUL but we'll overwrite it in the next strcpy
                    resp_ptr += uint_num_digits(res);
                    break;
                }
                default: break;
            }
        }

        strcpy(resp + resp_ptr, "}");
        resp_ptr += 1;

        if (i < num_fields - 1) {
            strcpy(resp + resp_ptr, ", ");
            resp_ptr += 2;
        }
    }

    strcpy(resp + resp_ptr, "]}");
    resp_ptr += 2;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    free(resp);
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

void browser_config_init(httpd_handle_t* server, nvs_handle_t* nvsHandle) {
    config_nvs_handle = *nvsHandle;
    ESP_LOGI(LOG_TAG, "Init");
    ESP_LOGI(LOG_TAG, "Registering URI handlers");
    httpd_register_uri_handler(*server, &config_get);
    httpd_register_uri_handler(*server, &config_get_fields);
    config_server = server;
}

void browser_config_deinit(void) {
    ESP_LOGI(LOG_TAG, "De-Init");
    ESP_LOGI(LOG_TAG, "Unregistering URI handlers");
    httpd_unregister_uri_handler(*config_server, config_get.uri, config_get.method);
    httpd_unregister_uri_handler(*config_server, config_get_fields.uri, config_get_fields.method);
}