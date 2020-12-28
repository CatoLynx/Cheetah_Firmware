#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>
#include "sys/param.h"

#include "browser_canvas.h"
#include "util_httpd.h"


#define LOG_TAG "Canvas"

static httpd_handle_t* canvas_server;
static uint8_t* canvas_output_buffer;
static size_t canvas_output_buffer_size = 0;

// Embedded files - refer to CMakeLists.txt
extern const uint8_t browser_canvas_html_start[] asm("_binary_browser_canvas_html_start");
extern const uint8_t browser_canvas_html_end[]   asm("_binary_browser_canvas_html_end");


static esp_err_t canvas_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)browser_canvas_html_start, browser_canvas_html_end - browser_canvas_html_start);
    return ESP_OK;
}

static esp_err_t canvas_update_post_handler(httpd_req_t *req) {
    return post_recv_handler(LOG_TAG, req, canvas_output_buffer, canvas_output_buffer_size);
}

static const httpd_uri_t canvas_get = {
    .uri       = "/canvas",
    .method    = HTTP_GET,
    .handler   = canvas_get_handler
};

static const httpd_uri_t canvas_update_post = {
    .uri       = "/canvas/update",
    .method    = HTTP_POST,
    .handler   = canvas_update_post_handler
};

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
    httpd_unregister_uri_handler(*canvas_server, canvas_get.uri, canvas_get.method);
    httpd_unregister_uri_handler(*canvas_server, canvas_update_post.uri, canvas_update_post.method);
}