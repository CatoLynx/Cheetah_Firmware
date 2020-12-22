#include "esp_log.h"
#include "sys/param.h"

#include "browser_ota.h"

#define LOG_TAG "HTTPD"


static esp_err_t hello_get_handler(httpd_req_t *req) {
    const char* str = "Hello World!";
    httpd_resp_send(req, str, strlen(str));
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler
};

httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_LOGI(LOG_TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(LOG_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        return server;
    }

    ESP_LOGI(LOG_TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server) {
    httpd_stop(server);
}