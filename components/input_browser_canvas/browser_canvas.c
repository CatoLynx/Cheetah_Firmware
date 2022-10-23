#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

#include "browser_canvas.h"
#include "util_httpd.h"

// TODO: Sometimes, the last change doesn't get sent


#define LOG_TAG "Canvas"

static httpd_handle_t* canvas_server;
static uint8_t* canvas_output_buffer;
static size_t canvas_output_buffer_size = 0;

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static uint8_t* canvas_brightness;
#endif

// Embedded files - refer to CMakeLists.txt
extern const uint8_t browser_canvas_html_start[] asm("_binary_browser_canvas_html_start");
extern const uint8_t browser_canvas_html_end[]   asm("_binary_browser_canvas_html_end");


static esp_err_t canvas_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)browser_canvas_html_start, browser_canvas_html_end - browser_canvas_html_start);
    return ESP_OK;
}

static esp_err_t canvas_update_post_handler(httpd_req_t *req) {
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    }

    cJSON* buffer_field = cJSON_GetObjectItem(json, "buffer");
    if (!cJSON_IsString(buffer_field)) {
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    }
    size_t b64_len = 0;
    char* buffer_str = cJSON_GetStringValue(buffer_field);
    size_t buffer_str_len = strlen(buffer_str);
    unsigned char* buffer_str_uchar = (unsigned char*)buffer_str;
    int result = mbedtls_base64_decode(NULL, 0, &b64_len, buffer_str_uchar, buffer_str_len);
    if (result == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
        // We don't cover MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL here
        // because this will always be returned when checking size
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    } else {
        b64_len = 0;
        result = mbedtls_base64_decode(canvas_output_buffer, canvas_output_buffer_size, &b64_len, buffer_str_uchar, buffer_str_len);
        if (result != 0) {
            cJSON_Delete(json);
            return abortRequest(req, "500 Internal Server Error");
        }
    }

    cJSON_Delete(json);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static esp_err_t canvas_set_brightness_post_handler(httpd_req_t *req) {
    ESP_LOGI(LOG_TAG, "Content length: %d bytes", req->content_len);

    char* buf = malloc(req->content_len);
    httpd_req_recv(req, buf, req->content_len);

    cJSON* json = cJSON_Parse(buf);
    free(buf);

    if (!cJSON_IsObject(json)) {
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    }

    cJSON* brightness_field = cJSON_GetObjectItem(json, "brightness");
    if (!cJSON_IsNumber(brightness_field)) {
        cJSON_Delete(json);
        return abortRequest(req, "500 Internal Server Error");
    }
    *canvas_brightness = (uint8_t)cJSON_GetNumberValue(brightness_field);

    cJSON_Delete(json);

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
#endif

static const httpd_uri_t canvas_get = {
    .uri       = "/canvas",
    .method    = HTTP_GET,
    .handler   = canvas_get_handler
};

static const httpd_uri_t canvas_update_post = {
    .uri       = "/canvas/update.json",
    .method    = HTTP_POST,
    .handler   = canvas_update_post_handler
};

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
static const httpd_uri_t canvas_set_brightness_post = {
    .uri       = "/canvas/brightness.json",
    .method    = HTTP_POST,
    .handler   = canvas_set_brightness_post_handler
};
#endif

void browser_canvas_init(httpd_handle_t* server, uint8_t* outBuf, size_t bufSize) {
    ESP_LOGI(LOG_TAG, "Starting browser canvas");
    canvas_output_buffer = outBuf;
    canvas_output_buffer_size = bufSize;
    httpd_register_uri_handler(*server, &canvas_get);
    httpd_register_uri_handler(*server, &canvas_update_post);
    canvas_server = server;
}

void browser_canvas_stop(void) {
    ESP_LOGI(LOG_TAG, "Stopping browser canvas");
    canvas_output_buffer = NULL;
    canvas_output_buffer_size = 0;
    httpd_unregister_uri_handler(*canvas_server, canvas_get.uri, canvas_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_update_post.uri, canvas_update_post.method);
    #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
    canvas_brightness = NULL;
    httpd_unregister_uri_handler(*canvas_server, canvas_set_brightness_post.uri, canvas_set_brightness_post.method);
    #endif
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
void browser_canvas_register_brightness(httpd_handle_t* server, uint8_t* brightness) {
    canvas_brightness = brightness;
    httpd_register_uri_handler(*server, &canvas_set_brightness_post);
}
#endif