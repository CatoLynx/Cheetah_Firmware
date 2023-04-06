#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "httpd.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_spiffs.h"
#include "wg.h"

#include "config_global.h"
#include "macros.h"

#define LOG_TAG "HTTPD"

// Embedded files - refer to CMakeLists.txt
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[]   asm("_binary_favicon_ico_end");
extern const uint8_t jquery_min_js_start[] asm("_binary_jquery_min_js_start");
extern const uint8_t jquery_min_js_end[]   asm("_binary_jquery_min_js_end");
extern const uint8_t util_js_start[] asm("_binary_util_js_start");
extern const uint8_t util_js_end[]   asm("_binary_util_js_end");
extern const uint8_t simple_css_start[] asm("_binary_simple_css_start");
extern const uint8_t simple_css_end[]   asm("_binary_simple_css_end");

extern char hostname[63];


static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/canvas");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

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

static esp_err_t util_js_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)util_js_start, (util_js_end - util_js_start)-1);
    return ESP_OK;
}

static esp_err_t simplecss_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)simple_css_start, (simple_css_end - simple_css_start)-1);
    return ESP_OK;
}

static esp_err_t device_info_get_handler(httpd_req_t *req) {
    tcpip_adapter_ip_info_t ip_info;
    tcpip_adapter_get_ip_info(ESP_IF_WIFI_STA, &ip_info);
    char ip_str[16];
    sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));

    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    esp_err_t ret = esp_ota_get_state_partition(partition, &ota_state);
    if (ret == ESP_ERR_NOT_SUPPORTED) {
        // Happens when trying to get the OTA state but we're running the factory app
        ota_state = ESP_OTA_IMG_VALID;
    } else {
        ESP_ERROR_CHECK(ret);
    }

    size_t spiffs_total = 0, spiffs_used = 0;
    ESP_ERROR_CHECK(esp_spiffs_info(SPIFFS_PARTITION_LABEL, &spiffs_total, &spiffs_used));

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "ip", ip_str);
    cJSON_AddStringToObject(json, "hostname", hostname);
    cJSON_AddStringToObject(json, "compile_date", __DATE__);
    cJSON_AddStringToObject(json, "compile_time", __TIME__);
    cJSON_AddBoolToObject  (json, "app_verified", (ota_state == ESP_OTA_IMG_VALID));
    cJSON_AddBoolToObject(json, "wireguard_up", wg_is_up());
    cJSON_AddNumberToObject(json, "spiffs_size", spiffs_total);
    cJSON_AddNumberToObject(json, "spiffs_free", spiffs_total - spiffs_used);

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static esp_err_t display_info_get_handler(httpd_req_t *req) {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "type", DISPLAY_TYPE);
    cJSON_AddStringToObject(json, "driver", DISPLAY_DRIVER);
#if defined(DISPLAY_HAS_SIZE)
    cJSON_AddNumberToObject(json, "width", CONFIG_DISPLAY_WIDTH);
    cJSON_AddNumberToObject(json, "height", CONFIG_DISPLAY_HEIGHT);
    cJSON_AddNumberToObject(json, "frame_width", DISPLAY_FRAME_WIDTH);
    cJSON_AddNumberToObject(json, "frame_height", DISPLAY_FRAME_HEIGHT);
    cJSON_AddNumberToObject(json, "viewport_offset_x", DISPLAY_VIEWPORT_OFFSET_X);
    cJSON_AddNumberToObject(json, "viewport_offset_y", DISPLAY_VIEWPORT_OFFSET_Y);
#else
    cJSON_AddNullToObject(json, "width");
    cJSON_AddNullToObject(json, "height");
    cJSON_AddNullToObject(json, "frame_width");
    cJSON_AddNullToObject(json, "frame_height");
    cJSON_AddNullToObject(json, "viewport_offset_x");
    cJSON_AddNullToObject(json, "viewport_offset_y");
#endif
    cJSON_AddNumberToObject(json, "framebuf_size", DISPLAY_FRAMEBUF_SIZE);
    cJSON_AddStringToObject(json, "frame_type", DISPLAY_FRAME_TYPE);
#if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    cJSON_AddNumberToObject(json, "charbuf_size", DISPLAY_CHARBUF_SIZE);
#else
    cJSON_AddNullToObject(json, "charbuf_size");
#endif
    cJSON_AddBoolToObject  (json, "brightness_control", DISPLAY_HAS_BRIGHTNESS_CONTROL);

    cJSON* quirks_arr = cJSON_CreateArray();
    cJSON* quirk_entry;

    #if defined(CONFIG_DISPLAY_QUIRKS_COMBINING_FULL_STOP)
    quirk_entry = cJSON_CreateString("combining_full_stop");
    cJSON_AddItemToArray(quirks_arr, quirk_entry);
    #endif

    cJSON_AddItemToObject(json, "quirks", quirks_arr);

    char *resp = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, resp, strlen(resp));
    cJSON_Delete(json);
    cJSON_free(resp);
    return ESP_OK;
}

static const httpd_uri_t root_get = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler
};

static const httpd_uri_t favicon_get = {
    .uri       = "/img/favicon.ico",
    .method    = HTTP_GET,
    .handler   = favicon_get_handler
};

static const httpd_uri_t jquery_get = {
    .uri       = "/js/jquery.min.js",
    .method    = HTTP_GET,
    .handler   = jquery_get_handler
};

static const httpd_uri_t util_js_get = {
    .uri       = "/js/util.js",
    .method    = HTTP_GET,
    .handler   = util_js_get_handler
};

static const httpd_uri_t simplecss_get = {
    .uri       = "/css/simple.css",
    .method    = HTTP_GET,
    .handler   = simplecss_get_handler
};

static const httpd_uri_t device_info_get = {
    .uri       = "/info/device.json",
    .method    = HTTP_GET,
    .handler   = device_info_get_handler
};

static const httpd_uri_t display_info_get = {
    .uri       = "/info/display.json",
    .method    = HTTP_GET,
    .handler   = display_info_get_handler
};

httpd_handle_t httpd_init(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.max_uri_handlers = 32;

    ESP_LOGI(LOG_TAG, "Starting HTTP server on port %d", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGI(LOG_TAG, "Error starting server!");
        return NULL;
    }

    ESP_LOGI(LOG_TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &root_get);
    httpd_register_uri_handler(server, &favicon_get);
    httpd_register_uri_handler(server, &jquery_get);
    httpd_register_uri_handler(server, &util_js_get);
    httpd_register_uri_handler(server, &simplecss_get);
    httpd_register_uri_handler(server, &device_info_get);
    httpd_register_uri_handler(server, &display_info_get);

    return server;
}

void httpd_deinit(httpd_handle_t server) {
    ESP_LOGI(LOG_TAG, "Unregistering URI handlers");
    httpd_unregister_uri_handler(server, root_get.uri, root_get.method);
    httpd_unregister_uri_handler(server, favicon_get.uri, favicon_get.method);
    httpd_unregister_uri_handler(server, jquery_get.uri, jquery_get.method);
    httpd_unregister_uri_handler(server, util_js_get.uri, util_js_get.method);
    httpd_unregister_uri_handler(server, simplecss_get.uri, simplecss_get.method);
    httpd_unregister_uri_handler(server, device_info_get.uri, device_info_get.method);
    httpd_unregister_uri_handler(server, display_info_get.uri, display_info_get.method);
    ESP_LOGI(LOG_TAG, "Stopping HTTP server");
    httpd_stop(server);
}