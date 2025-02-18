#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "mbedtls/base64.h"

#include "macros.h"
#include "browser_canvas.h"
#include "util_buffer.h"
#include "util_httpd.h"
#include "util_nvs.h"
#include "settings_secret.h"

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
#include SHADERS_INCLUDE
#endif

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
#include TRANSITIONS_INCLUDE
#endif

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
#include EFFECTS_INCLUDE
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    #include "bitmap_generators.h"
#endif

// TODO: Sometimes, the last change doesn't get sent


#define LOG_TAG "Canvas"

extern portMUX_TYPE display_pixel_buffer_lock;

static httpd_handle_t* canvas_server;
static nvs_handle_t canvas_nvs_handle;
static basic_auth_info_t* basic_auth_info;
static uint8_t canvas_use_auth = 0;
static uint8_t* canvas_pixel_buffer = NULL;
static size_t canvas_pixel_buffer_size = 0;
static portMUX_TYPE* canvas_pixel_buffer_lock = NULL;
static uint8_t* canvas_text_buffer = NULL;
static size_t canvas_text_buffer_size = 0;
static portMUX_TYPE* canvas_text_buffer_lock = NULL;
static uint8_t* canvas_unit_buffer = NULL;
static size_t canvas_unit_buffer_size = 0;
static portMUX_TYPE* canvas_unit_buffer_lock = NULL;

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static uint8_t* canvas_brightness = NULL;
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

static cJSON* canvas_presets = NULL;

// Embedded files - refer to CMakeLists.txt
extern const uint8_t browser_canvas_html_start[] asm("_binary_browser_canvas_html_start");
extern const uint8_t browser_canvas_html_end[]   asm("_binary_browser_canvas_html_end");


static esp_err_t canvas_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)browser_canvas_html_start, browser_canvas_html_end - browser_canvas_html_start);
    return ESP_OK;
}

static esp_err_t canvas_pixel_buffer_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;

    unsigned char* b64_buf = NULL;
    esp_err_t ret = buffer_to_base64(canvas_pixel_buffer, canvas_pixel_buffer_size, &b64_buf);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (char*)b64_buf, strlen((char*)b64_buf));
        free(b64_buf);
        return ESP_OK;
    } else {
        return abortRequest(req, HTTPD_404);
    }
}

static esp_err_t canvas_text_buffer_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    unsigned char* b64_buf = NULL;
    esp_err_t ret = buffer_to_base64(canvas_text_buffer, canvas_text_buffer_size, &b64_buf);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (char*)b64_buf, strlen((char*)b64_buf));
        free(b64_buf);
        return ESP_OK;
    } else {
        return abortRequest(req, HTTPD_404);
    }
}

static esp_err_t canvas_unit_buffer_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    unsigned char* b64_buf = NULL;
    esp_err_t ret = buffer_to_base64(canvas_unit_buffer, canvas_unit_buffer_size, &b64_buf);

    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_send(req, (char*)b64_buf, strlen((char*)b64_buf));
        free(b64_buf);
        return ESP_OK;
    } else {
        return abortRequest(req, HTTPD_404);
    }
}

static esp_err_t canvas_pixel_buffer_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    if (canvas_pixel_buffer == NULL) return abortRequest(req, HTTPD_404);

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);
    char* buf = malloc(req->content_len + 1);
    int bytesRead = 0;
    while (bytesRead < req->content_len) {
        int ret = httpd_req_recv(req, &buf[bytesRead], req->content_len - bytesRead);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(LOG_TAG, "Socket timeout, continuing");
                continue;
            }
            ESP_LOGI(LOG_TAG, "Receive error, aborting");
            return abortRequest(req, HTTPD_500);
        }
        bytesRead += ret;
    }
    buf[req->content_len] = 0;

    size_t b64_len = 0;
    size_t buffer_str_len = strlen(buf);
    unsigned char* buffer_str_uchar = (unsigned char*)buf;
    int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
    if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
        // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
        // because this will always be returned when checking size
        free(buf);
        return abortRequest(req, HTTPD_500);
    } else {
        b64_len = 0;
        taskENTER_CRITICAL(canvas_pixel_buffer_lock);
        result = mbedtls_base64_decode(canvas_pixel_buffer, canvas_pixel_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
        taskEXIT_CRITICAL(canvas_pixel_buffer_lock);
        if (result != 0) {
            free(buf);
            ESP_LOGI(LOG_TAG, "result != 0");
            return abortRequest(req, HTTPD_500);
        }
    }

    free(buf);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t canvas_text_buffer_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    if (canvas_text_buffer == NULL) return abortRequest(req, HTTPD_404);

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);
    char* buf = malloc(req->content_len + 1);
    int bytesRead = 0;
    while (bytesRead < req->content_len) {
        int ret = httpd_req_recv(req, &buf[bytesRead], req->content_len - bytesRead);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(LOG_TAG, "Socket timeout, continuing");
                continue;
            }
            ESP_LOGI(LOG_TAG, "Receive error, aborting");
            return abortRequest(req, HTTPD_500);
        }
        bytesRead += ret;
    }
    buf[req->content_len] = 0;

    size_t b64_len = 0;
    size_t buffer_str_len = strlen(buf);
    unsigned char* buffer_str_uchar = (unsigned char*)buf;
    int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
    if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
        // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
        // because this will always be returned when checking size
        free(buf);
        return abortRequest(req, HTTPD_500);
    } else {
        b64_len = 0;
        taskENTER_CRITICAL(canvas_text_buffer_lock);
        result = mbedtls_base64_decode(canvas_text_buffer, canvas_text_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
        taskEXIT_CRITICAL(canvas_text_buffer_lock);
        if (b64_len < canvas_text_buffer_size) canvas_text_buffer[b64_len] = 0; // Ensure null termination
        if (result != 0) {
            free(buf);
            ESP_LOGI(LOG_TAG, "result != 0");
            return abortRequest(req, HTTPD_500);
        }
    }

    free(buf);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t canvas_unit_buffer_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    if (canvas_unit_buffer == NULL) return abortRequest(req, HTTPD_404);

    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);
    char* buf = malloc(req->content_len + 1);
    int bytesRead = 0;
    while (bytesRead < req->content_len) {
        int ret = httpd_req_recv(req, &buf[bytesRead], req->content_len - bytesRead);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                ESP_LOGI(LOG_TAG, "Socket timeout, continuing");
                continue;
            }
            ESP_LOGI(LOG_TAG, "Receive error, aborting");
            return abortRequest(req, HTTPD_500);
        }
        bytesRead += ret;
    }
    buf[req->content_len] = 0;

    size_t b64_len = 0;
    size_t buffer_str_len = strlen(buf);
    unsigned char* buffer_str_uchar = (unsigned char*)buf;
    int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
    if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
        // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
        // because this will always be returned when checking size
        free(buf);
        return abortRequest(req, HTTPD_500);
    } else {
        b64_len = 0;
        taskENTER_CRITICAL(canvas_unit_buffer_lock);
        result = mbedtls_base64_decode(canvas_unit_buffer, canvas_unit_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
        taskEXIT_CRITICAL(canvas_unit_buffer_lock);
        if (result != 0) {
            free(buf);
            return abortRequest(req, HTTPD_500);
        }
    }

    free(buf);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static esp_err_t canvas_brightness_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "brightness", *canvas_brightness);

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t canvas_brightness_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    cJSON* brightness_field = cJSON_GetObjectItem(json, "brightness");
    if (!cJSON_IsNumber(brightness_field)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }
    *canvas_brightness = (uint8_t)cJSON_GetNumberValue(brightness_field);

    cJSON* default_field = cJSON_GetObjectItem(json, "saveDefault");
    if (cJSON_IsBool(default_field)) {
        if (cJSON_IsTrue(default_field)) {
            nvs_set_u8(canvas_nvs_handle, "deflt_bright", *canvas_brightness);
        }
    }

    cJSON_Delete(json);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

static esp_err_t canvas_get_shaders_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    #if defined(CONFIG_DISPLAY_HAS_SHADERS)
    cJSON* json = shader_get_available();
    #else
    cJSON* json = cJSON_CreateObject();
    #endif

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
static esp_err_t canvas_shader_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    char* resp;
    if (shader_data == NULL || *shader_data == NULL) {
        cJSON* json = cJSON_CreateObject();
        resp = cJSON_Print(json);
        cJSON_Delete(json);
    } else {
        resp = cJSON_Print(*shader_data);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t canvas_shader_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    *shader_data = json;

    // Tell the main loop that the currently set data
    // can be deleted once it's not in use anymore
    *shader_data_deletable = 1;

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

static esp_err_t canvas_get_transitions_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
    cJSON* json = transition_get_available();
    #else
    cJSON* json = cJSON_CreateObject();
    #endif

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
static esp_err_t canvas_transition_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    char* resp;
    if (transition_data == NULL || *transition_data == NULL) {
        cJSON* json = cJSON_CreateObject();
        resp = cJSON_Print(json);
        cJSON_Delete(json);
    } else {
        resp = cJSON_Print(*transition_data);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t canvas_transition_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    *transition_data = json;
    
    // Tell the main loop that the currently set data
    // can be deleted once it's not in use anymore
    *transition_data_deletable = 1;

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

static esp_err_t canvas_get_effects_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
    cJSON* json = effect_get_available();
    #else
    cJSON* json = cJSON_CreateObject();
    #endif

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
static esp_err_t canvas_effect_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    char* resp;
    if (effect_data == NULL || *effect_data == NULL) {
        cJSON* json = cJSON_CreateObject();
        resp = cJSON_Print(json);
        cJSON_Delete(json);
    } else {
        resp = cJSON_Print(*effect_data);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t canvas_effect_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    *effect_data = json;
    
    // Tell the main loop that the currently set data
    // can be deleted once it's not in use anymore
    *effect_data_deletable = 1;

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

static esp_err_t canvas_get_bitmap_generators_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    #if defined(DISPLAY_HAS_PIXEL_BUFFER)
    cJSON* json = bitmap_generators_get_available();
    #else
    cJSON* json = cJSON_CreateObject();
    #endif

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
static esp_err_t canvas_bitmap_generator_get_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    char* resp;
    if (bitmap_generator_data == NULL || *bitmap_generator_data == NULL) {
        cJSON* json = cJSON_CreateObject();
        resp = cJSON_Print(json);
        cJSON_Delete(json);
    } else {
        resp = cJSON_Print(*bitmap_generator_data);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t canvas_bitmap_generator_post_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;
    
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, HTTPD_500);
    }

    *bitmap_generator_data = json;
    
    // Tell the main loop that the currently set data
    // can be deleted once it's not in use anymore
    *bitmap_generator_data_deletable = 1;

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

static esp_err_t canvas_get_presets_handler(httpd_req_t *req) {
    if (canvas_use_auth) if (!basic_auth_handler(req, LOG_TAG)) return ESP_OK;

    char* resp;

    if (canvas_presets == NULL) {
        cJSON* json = cJSON_CreateObject();
        resp = cJSON_Print(json);
        cJSON_Delete(json);
    } else {
        resp = cJSON_Print(canvas_presets);
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t save_startup_post_handler(httpd_req_t *req) {
    uint8_t success = 1;
    cJSON* json = cJSON_CreateObject();
    cJSON* startupData = cJSON_CreateObject();

    #if defined(CONFIG_DISPLAY_HAS_SHADERS)
    if (shader_data != NULL) {
        if (cJSON_IsObject(*shader_data)) {
            cJSON_AddItemToObject(startupData, "shader", cJSON_Duplicate(*shader_data, true));
        }
    }
    #endif

    #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
    if (transition_data != NULL) {
        if (cJSON_IsObject(*transition_data)) {
            cJSON_AddItemToObject(startupData, "transition", cJSON_Duplicate(*transition_data, true));
        }
    }
    #endif

    #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
    if (effect_data != NULL) {
        if (cJSON_IsObject(*effect_data)) {
            cJSON_AddItemToObject(startupData, "effect", cJSON_Duplicate(*effect_data, true));
        }
    }
    #endif

    #if defined(DISPLAY_HAS_PIXEL_BUFFER)
    if (bitmap_generator_data != NULL) {
        if (cJSON_IsObject(*bitmap_generator_data)) {
            cJSON_AddItemToObject(startupData, "bitmap_generator", cJSON_Duplicate(*bitmap_generator_data, true));
        }
    }
    #endif

    cJSON* buffers_field = cJSON_CreateObject();

    if (canvas_pixel_buffer != NULL) {
        unsigned char* pixel_b64 = NULL;
        esp_err_t status = buffer_to_base64(canvas_pixel_buffer, canvas_pixel_buffer_size, &pixel_b64);
        if (status == ESP_OK) {
            cJSON_AddStringToObject(buffers_field, "pixel_b64", (char*)pixel_b64);
            free(pixel_b64);
        }
    }

    if (canvas_text_buffer != NULL) {
        unsigned char* text_b64 = NULL;
        esp_err_t status = buffer_to_base64(canvas_text_buffer, canvas_text_buffer_size, &text_b64);
        if (status == ESP_OK) {
            cJSON_AddStringToObject(buffers_field, "text_b64", (char*)text_b64);
            free(text_b64);
        }
    }

    if (canvas_unit_buffer != NULL) {
        unsigned char* unit_b64 = NULL;
        esp_err_t status = buffer_to_base64(canvas_unit_buffer, canvas_unit_buffer_size, &unit_b64);
        if (status == ESP_OK) {
            cJSON_AddStringToObject(buffers_field, "unit_b64", (char*)unit_b64);
            free(unit_b64);
        }
    }

    cJSON_AddItemToObject(startupData, "buffers", buffers_field);

    char* startupFile = get_string_from_nvs(&canvas_nvs_handle, "startup_file");
    if (startupFile == NULL) {
        success = 0;
        ESP_LOGW(LOG_TAG, "Not using startup file");
    } else {
        esp_err_t ret = save_json_to_spiffs(startupFile, startupData, LOG_TAG);
        ESP_LOGI(LOG_TAG, "Saved");
        if (ret != ESP_OK) {
            ESP_LOGE(LOG_TAG, "Failed to save startup file");
            success = 0;
        }
    }
    cJSON_Delete(startupData);

    cJSON_AddBoolToObject(json, "success", success);

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static httpd_uri_t canvas_get = {
    .uri       = "/canvas",
    .method    = HTTP_GET,
    .handler   = canvas_get_handler
};

static httpd_uri_t canvas_pixel_buffer_get = {
    .uri       = "/canvas/buffer/pixel",
    .method    = HTTP_GET,
    .handler   = canvas_pixel_buffer_get_handler
};

static httpd_uri_t canvas_text_buffer_get = {
    .uri       = "/canvas/buffer/text",
    .method    = HTTP_GET,
    .handler   = canvas_text_buffer_get_handler
};

static httpd_uri_t canvas_unit_buffer_get = {
    .uri       = "/canvas/buffer/unit",
    .method    = HTTP_GET,
    .handler   = canvas_unit_buffer_get_handler
};

static httpd_uri_t canvas_pixel_buffer_post = {
    .uri       = "/canvas/buffer/pixel",
    .method    = HTTP_POST,
    .handler   = canvas_pixel_buffer_post_handler
};

static httpd_uri_t canvas_text_buffer_post = {
    .uri       = "/canvas/buffer/text",
    .method    = HTTP_POST,
    .handler   = canvas_text_buffer_post_handler
};

static httpd_uri_t canvas_unit_buffer_post = {
    .uri       = "/canvas/buffer/unit",
    .method    = HTTP_POST,
    .handler   = canvas_unit_buffer_post_handler
};

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static httpd_uri_t canvas_brightness_get = {
    .uri       = "/canvas/brightness.json",
    .method    = HTTP_GET,
    .handler   = canvas_brightness_get_handler
};

static httpd_uri_t canvas_brightness_post = {
    .uri       = "/canvas/brightness.json",
    .method    = HTTP_POST,
    .handler   = canvas_brightness_post_handler
};
#endif

static httpd_uri_t canvas_get_shaders = {
    .uri       = "/canvas/shaders.json",
    .method    = HTTP_GET,
    .handler   = canvas_get_shaders_handler
};

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
static httpd_uri_t canvas_shader_get = {
    .uri       = "/canvas/shader.json",
    .method    = HTTP_GET,
    .handler   = canvas_shader_get_handler
};

static httpd_uri_t canvas_shader_post = {
    .uri       = "/canvas/shader.json",
    .method    = HTTP_POST,
    .handler   = canvas_shader_post_handler
};
#endif

static httpd_uri_t canvas_get_transitions = {
    .uri       = "/canvas/transitions.json",
    .method    = HTTP_GET,
    .handler   = canvas_get_transitions_handler
};

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
static httpd_uri_t canvas_transition_get = {
    .uri       = "/canvas/transition.json",
    .method    = HTTP_GET,
    .handler   = canvas_transition_get_handler
};

static httpd_uri_t canvas_transition_post = {
    .uri       = "/canvas/transition.json",
    .method    = HTTP_POST,
    .handler   = canvas_transition_post_handler
};
#endif

static httpd_uri_t canvas_get_effects = {
    .uri       = "/canvas/effects.json",
    .method    = HTTP_GET,
    .handler   = canvas_get_effects_handler
};

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
static httpd_uri_t canvas_effect_get = {
    .uri       = "/canvas/effect.json",
    .method    = HTTP_GET,
    .handler   = canvas_effect_get_handler
};

static httpd_uri_t canvas_effect_post = {
    .uri       = "/canvas/effect.json",
    .method    = HTTP_POST,
    .handler   = canvas_effect_post_handler
};
#endif

static httpd_uri_t canvas_get_bitmap_generators = {
    .uri       = "/canvas/bitmap_generators.json",
    .method    = HTTP_GET,
    .handler   = canvas_get_bitmap_generators_handler
};

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
static httpd_uri_t canvas_bitmap_generator_get = {
    .uri       = "/canvas/bitmap_generator.json",
    .method    = HTTP_GET,
    .handler   = canvas_bitmap_generator_get_handler
};

static httpd_uri_t canvas_bitmap_generator_post = {
    .uri       = "/canvas/bitmap_generator.json",
    .method    = HTTP_POST,
    .handler   = canvas_bitmap_generator_post_handler
};
#endif

static httpd_uri_t canvas_get_presets = {
    .uri       = "/canvas/presets.json",
    .method    = HTTP_GET,
    .handler   = canvas_get_presets_handler
};

static const httpd_uri_t canvas_save_startup_post = {
    .uri       = "/canvas/saveStartup.json",
    .method    = HTTP_POST,
    .handler   = save_startup_post_handler
};

void browser_canvas_init(httpd_handle_t* server, nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock, uint8_t* textBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* unitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock) {
    ESP_LOGI(LOG_TAG, "Starting browser canvas");
    canvas_nvs_handle = *nvsHandle;
    canvas_pixel_buffer = pixBuf;
    canvas_pixel_buffer_size = pixBufSize;
    canvas_pixel_buffer_lock = pixBufLock;
    canvas_text_buffer = textBuf;
    canvas_text_buffer_size = textBufSize;
    canvas_text_buffer_lock = textBufLock;
    canvas_unit_buffer = unitBuf;
    canvas_unit_buffer_size = unitBufSize;
    canvas_unit_buffer_lock = unitBufLock;

    basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    basic_auth_info->username = HTTPD_CONFIG_USERNAME;
    basic_auth_info->password = HTTPD_CONFIG_PASSWORD;
    basic_auth_info->realm    = "Cheetah Canvas";

    esp_err_t ret = nvs_get_u8(canvas_nvs_handle, "canvas_use_auth", &canvas_use_auth);
    if (ret != ESP_OK) canvas_use_auth = 0;

    if (canvas_use_auth) {
        ESP_LOGI(LOG_TAG, "Using authentication");
    } else {
        ESP_LOGI(LOG_TAG, "Not using authentication");
    }
    
    if (canvas_use_auth) {
        canvas_get.user_ctx = basic_auth_info;
        canvas_pixel_buffer_get.user_ctx = basic_auth_info;
        canvas_text_buffer_get.user_ctx = basic_auth_info;
        canvas_unit_buffer_get.user_ctx = basic_auth_info;
        canvas_pixel_buffer_post.user_ctx = basic_auth_info;
        canvas_text_buffer_post.user_ctx = basic_auth_info;
        canvas_unit_buffer_post.user_ctx = basic_auth_info;
        canvas_get_shaders.user_ctx = basic_auth_info;
        canvas_get_transitions.user_ctx = basic_auth_info;
        canvas_get_effects.user_ctx = basic_auth_info;
        canvas_get_bitmap_generators.user_ctx = basic_auth_info;
        canvas_get_presets.user_ctx = basic_auth_info;
        
        #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
        canvas_brightness_get.user_ctx = basic_auth_info;
        canvas_brightness_post.user_ctx = basic_auth_info;
        #endif
        
        #if defined(CONFIG_DISPLAY_HAS_SHADERS)
        canvas_shader_get.user_ctx = basic_auth_info;
        canvas_shader_post.user_ctx = basic_auth_info;
        #endif
        
        #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
        canvas_transition_get.user_ctx = basic_auth_info;
        canvas_transition_post.user_ctx = basic_auth_info;
        #endif
        
        #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
        canvas_effect_get.user_ctx = basic_auth_info;
        canvas_effect_post.user_ctx = basic_auth_info;
        #endif
        
        #if defined(DISPLAY_HAS_PIXEL_BUFFER)
        canvas_bitmap_generator_get.user_ctx = basic_auth_info;
        canvas_bitmap_generator_post.user_ctx = basic_auth_info;
        #endif
    }

    char* presetFile = get_string_from_nvs(nvsHandle, "cnv_preset_file");
    if (presetFile == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to get preset file name from NVS");
    } else {
        get_json_from_spiffs(presetFile, &canvas_presets, LOG_TAG);
    }
    free(presetFile);

    httpd_register_uri_handler(*server, &canvas_get);
    httpd_register_uri_handler(*server, &canvas_pixel_buffer_get);
    httpd_register_uri_handler(*server, &canvas_text_buffer_get);
    httpd_register_uri_handler(*server, &canvas_unit_buffer_get);
    httpd_register_uri_handler(*server, &canvas_pixel_buffer_post);
    httpd_register_uri_handler(*server, &canvas_text_buffer_post);
    httpd_register_uri_handler(*server, &canvas_unit_buffer_post);
    httpd_register_uri_handler(*server, &canvas_get_shaders);
    httpd_register_uri_handler(*server, &canvas_get_transitions);
    httpd_register_uri_handler(*server, &canvas_get_effects);
    httpd_register_uri_handler(*server, &canvas_get_bitmap_generators);
    httpd_register_uri_handler(*server, &canvas_get_presets);
    httpd_register_uri_handler(*server, &canvas_save_startup_post);
    canvas_server = server;
}

void browser_canvas_stop(void) {
    ESP_LOGI(LOG_TAG, "Stopping browser canvas");
    canvas_pixel_buffer = NULL;
    canvas_pixel_buffer_size = 0;
    canvas_text_buffer = NULL;
    canvas_text_buffer_size = 0;
    canvas_unit_buffer = NULL;
    canvas_unit_buffer_size = 0;
    httpd_unregister_uri_handler(*canvas_server, canvas_get.uri, canvas_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_pixel_buffer_get.uri, canvas_pixel_buffer_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_text_buffer_get.uri, canvas_text_buffer_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_unit_buffer_get.uri, canvas_unit_buffer_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_pixel_buffer_post.uri, canvas_pixel_buffer_post.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_text_buffer_post.uri, canvas_text_buffer_post.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_unit_buffer_post.uri, canvas_unit_buffer_post.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_get_shaders.uri, canvas_get_shaders.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_get_transitions.uri, canvas_get_transitions.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_get_effects.uri, canvas_get_effects.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_get_bitmap_generators.uri, canvas_get_bitmap_generators.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_get_presets.uri, canvas_get_presets.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_save_startup_post.uri, canvas_save_startup_post.method);
    #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
    canvas_brightness = NULL;
    httpd_unregister_uri_handler(*canvas_server, canvas_brightness_get.uri, canvas_brightness_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_brightness_post.uri, canvas_brightness_post.method);
    #endif
    #if defined(CONFIG_DISPLAY_HAS_SHADERS)
    shader_data = NULL;
    httpd_unregister_uri_handler(*canvas_server, canvas_shader_get.uri, canvas_shader_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_shader_post.uri, canvas_shader_post.method);
    #endif
    #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
    transition_data = NULL;
    httpd_unregister_uri_handler(*canvas_server, canvas_transition_get.uri, canvas_transition_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_transition_post.uri, canvas_transition_post.method);
    #endif
    #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
    effect_data = NULL;
    httpd_unregister_uri_handler(*canvas_server, canvas_effect_get.uri, canvas_effect_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_effect_post.uri, canvas_effect_post.method);
    #endif
    #if defined(DISPLAY_HAS_PIXEL_BUFFER)
    bitmap_generator_data = NULL;
    httpd_unregister_uri_handler(*canvas_server, canvas_bitmap_generator_get.uri, canvas_bitmap_generator_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_bitmap_generator_post.uri, canvas_bitmap_generator_post.method);
    #endif
    free(basic_auth_info);
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
void browser_canvas_register_brightness(httpd_handle_t* server, uint8_t* brightness) {
    canvas_brightness = brightness;
    esp_err_t ret = nvs_get_u8(canvas_nvs_handle, "deflt_bright", canvas_brightness);
    if (ret != ESP_OK) *canvas_brightness = 255;
    httpd_register_uri_handler(*server, &canvas_brightness_get);
    httpd_register_uri_handler(*server, &canvas_brightness_post);
}
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
void browser_canvas_register_shaders(httpd_handle_t* server, cJSON** shaderData, uint8_t* shaderDataDeletable) {
    shader_data = shaderData;
    shader_data_deletable = shaderDataDeletable;
    httpd_register_uri_handler(*server, &canvas_shader_get);
    httpd_register_uri_handler(*server, &canvas_shader_post);
}
#endif

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
void browser_canvas_register_transitions(httpd_handle_t* server, cJSON** transitionData, uint8_t* transitionDataDeletable) {
    transition_data = transitionData;
    transition_data_deletable = transitionDataDeletable;
    httpd_register_uri_handler(*server, &canvas_transition_get);
    httpd_register_uri_handler(*server, &canvas_transition_post);
}
#endif

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
void browser_canvas_register_effects(httpd_handle_t* server, cJSON** effectData, uint8_t* effectDataDeletable) {
    effect_data = effectData;
    effect_data_deletable = effectDataDeletable;
    httpd_register_uri_handler(*server, &canvas_effect_get);
    httpd_register_uri_handler(*server, &canvas_effect_post);
}
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
void browser_canvas_register_bitmap_generators(httpd_handle_t* server, cJSON** bitmapGeneratorData, uint8_t* bitmapGeneratorDataDeletable) {
    bitmap_generator_data = bitmapGeneratorData;
    bitmap_generator_data_deletable = bitmapGeneratorDataDeletable;
    httpd_register_uri_handler(*server, &canvas_bitmap_generator_get);
    httpd_register_uri_handler(*server, &canvas_bitmap_generator_post);
}
#endif