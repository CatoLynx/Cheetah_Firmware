#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "mbedtls/base64.h"

#include "macros.h"
#include "browser_canvas.h"
#include "util_httpd.h"
#include "settings_secret.h"

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
#include SHADER_INCLUDE
#endif

// TODO: Sometimes, the last change doesn't get sent


#define LOG_TAG "Canvas"

static httpd_handle_t* canvas_server;
static nvs_handle_t canvas_nvs_handle;
static basic_auth_info_t* basic_auth_info;
static uint8_t canvas_use_auth = 0;
static uint8_t* canvas_pixel_buffer = NULL;
static size_t canvas_pixel_buffer_size = 0;
static uint8_t* canvas_text_buffer = NULL;
static size_t canvas_text_buffer_size = 0;
static uint8_t* canvas_unit_buffer = NULL;
static size_t canvas_unit_buffer_size = 0;

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static uint8_t* canvas_brightness;
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
static cJSON** shader_data;
#endif

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

    if (canvas_pixel_buffer != NULL) {
        size_t b64_len = 0;
        mbedtls_base64_encode(NULL, 0, &b64_len, canvas_pixel_buffer, canvas_pixel_buffer_size);
        size_t b64_bufsize = b64_len;
        b64_buf = malloc(b64_len);
        b64_len = 0;
        mbedtls_base64_encode(b64_buf, b64_bufsize, &b64_len, canvas_pixel_buffer, canvas_pixel_buffer_size);
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

    if (canvas_text_buffer != NULL) {
        size_t b64_len = 0;
        mbedtls_base64_encode(NULL, 0, &b64_len, canvas_text_buffer, canvas_text_buffer_size);
        size_t b64_bufsize = b64_len;
        b64_buf = malloc(b64_len);
        b64_len = 0;
        mbedtls_base64_encode(b64_buf, b64_bufsize, &b64_len, canvas_text_buffer, canvas_text_buffer_size);
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

    if (canvas_unit_buffer != NULL) {
        size_t b64_len = 0;
        mbedtls_base64_encode(NULL, 0, &b64_len, canvas_unit_buffer, canvas_unit_buffer_size);
        size_t b64_bufsize = b64_len;
        b64_buf = malloc(b64_len);
        b64_len = 0;
        mbedtls_base64_encode(b64_buf, b64_bufsize, &b64_len, canvas_unit_buffer, canvas_unit_buffer_size);
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
        result = mbedtls_base64_decode(canvas_pixel_buffer, canvas_pixel_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
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
        result = mbedtls_base64_decode(canvas_text_buffer, canvas_text_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
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
        result = mbedtls_base64_decode(canvas_unit_buffer, canvas_unit_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
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

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

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

void browser_canvas_init(httpd_handle_t* server, nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, uint8_t* textBuf, size_t textBufSize, uint8_t* unitBuf, size_t unitBufSize) {
    ESP_LOGI(LOG_TAG, "Starting browser canvas");
    canvas_nvs_handle = *nvsHandle;
    canvas_pixel_buffer = pixBuf;
    canvas_pixel_buffer_size = pixBufSize;
    canvas_text_buffer = textBuf;
    canvas_text_buffer_size = textBufSize;
    canvas_unit_buffer = unitBuf;
    canvas_unit_buffer_size = unitBufSize;

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
        
        #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
        canvas_brightness_get.user_ctx = basic_auth_info;
        canvas_brightness_post.user_ctx = basic_auth_info;
        #endif
        
        #if defined(CONFIG_DISPLAY_HAS_SHADERS)
        canvas_shader_get.user_ctx = basic_auth_info;
        canvas_shader_post.user_ctx = basic_auth_info;
        #endif
    }

    httpd_register_uri_handler(*server, &canvas_get);
    httpd_register_uri_handler(*server, &canvas_pixel_buffer_get);
    httpd_register_uri_handler(*server, &canvas_text_buffer_get);
    httpd_register_uri_handler(*server, &canvas_unit_buffer_get);
    httpd_register_uri_handler(*server, &canvas_pixel_buffer_post);
    httpd_register_uri_handler(*server, &canvas_text_buffer_post);
    httpd_register_uri_handler(*server, &canvas_unit_buffer_post);
    httpd_register_uri_handler(*server, &canvas_get_shaders);
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
void browser_canvas_register_shaders(httpd_handle_t* server, cJSON** shaderData) {
    shader_data = shaderData;
    httpd_register_uri_handler(*server, &canvas_shader_get);
    httpd_register_uri_handler(*server, &canvas_shader_post);
}
#endif