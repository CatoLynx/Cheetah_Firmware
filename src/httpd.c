#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "httpd.h"

#define LOG_TAG "HTTPD"

// Embedded files - refer to CMakeLists.txt
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");
extern const uint8_t jquery_min_js_start[] asm("_binary_jquery_min_js_start");
extern const uint8_t jquery_min_js_end[]   asm("_binary_jquery_min_js_end");


static esp_err_t favicon_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);
    return ESP_OK;
}

static esp_err_t jquery_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)jquery_min_js_start, (jquery_min_js_end - jquery_min_js_start)-1);
    return ESP_OK;
}

static esp_err_t ip_get_handler(httpd_req_t *req) {
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip_info);
    char ip_str[16];
    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, ip_str, strlen(ip_str));
    return ESP_OK;
}

static const httpd_uri_t favicon_get = {
    .uri       = "/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler
};

static const httpd_uri_t jquery_get = {
    .uri       = "/jquery.min.js",
    .method    = HTTP_GET,
    .handler   = jquery_get_handler
};

static const httpd_uri_t ip_get = {
    .uri       = "/ip",
    .method    = HTTP_GET,
    .handler   = ip_get_handler
};

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_uri_handlers = 16;

    ESP_LOGI(LOG_TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGI(LOG_TAG, "Error starting server!");
        return NULL;
    }

    ESP_LOGI(LOG_TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &favicon_get);
    httpd_register_uri_handler(server, &jquery_get);
    httpd_register_uri_handler(server, &ip_get);

    return server;
}

void stop_webserver(httpd_handle_t server) {
    ESP_LOGI(LOG_TAG, "Stopping HTTP server");
    httpd_stop(server);
}