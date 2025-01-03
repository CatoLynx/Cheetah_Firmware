#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "httpd.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "esp_spiffs.h"
#include "esp_mac.h"
#include "wg.h"
#include "git_version.h"

#include "config_global.h"
#include "util_disp_selection.h"

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

extern esp_netif_t* netif_wifi_sta;
extern esp_netif_t* netif_wifi_ap;
extern esp_netif_t* netif_eth;

extern char hostname[63];

nvs_handle_t httpd_nvs_handle;


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
    esp_netif_ip_info_t ip_info;
    char ip_str[16];
    cJSON* ips = cJSON_CreateObject();

    if (netif_wifi_sta != NULL) {
        memset(&ip_info, 0x00, sizeof(esp_netif_ip_info_t));
        esp_netif_get_ip_info(netif_wifi_sta, &ip_info);
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
        cJSON_AddStringToObject(ips, "wifi_sta", ip_str);
    } else {
        cJSON_AddStringToObject(ips, "wifi_sta", "0.0.0.0");
    }

    if (netif_wifi_ap != NULL) {
        memset(&ip_info, 0x00, sizeof(esp_netif_ip_info_t));
        esp_netif_get_ip_info(netif_wifi_ap, &ip_info);
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
        cJSON_AddStringToObject(ips, "wifi_ap", ip_str);
    } else {
        cJSON_AddStringToObject(ips, "wifi_ap", "0.0.0.0");
    }

    if (netif_eth != NULL) {
        memset(&ip_info, 0x00, sizeof(esp_netif_ip_info_t));
        esp_netif_get_ip_info(netif_eth, &ip_info);
        sprintf(ip_str, IPSTR, IP2STR(&ip_info.ip));
        cJSON_AddStringToObject(ips, "eth", ip_str);
    } else {
        cJSON_AddStringToObject(ips, "eth", "0.0.0.0");
    }

    uint8_t mac_bytes[6];
    char mac_str[18];
    cJSON* macs = cJSON_CreateObject();
    memset(mac_str, 0x00, 18);
    esp_read_mac(mac_bytes, ESP_MAC_WIFI_STA);
    sprintf(mac_str, MACSTR, MAC2STR(mac_bytes));
    cJSON_AddStringToObject(macs, "wifi_sta", mac_str);
    memset(mac_str, 0x00, 18);
    esp_read_mac(mac_bytes, ESP_MAC_WIFI_SOFTAP);
    sprintf(mac_str, MACSTR, MAC2STR(mac_bytes));
    cJSON_AddStringToObject(macs, "wifi_ap", mac_str);
    memset(mac_str, 0x00, 18);
    esp_read_mac(mac_bytes, ESP_MAC_BT);
    sprintf(mac_str, MACSTR, MAC2STR(mac_bytes));
    cJSON_AddStringToObject(macs, "bt", mac_str);
    memset(mac_str, 0x00, 18);
    esp_read_mac(mac_bytes, ESP_MAC_ETH);
    sprintf(mac_str, MACSTR, MAC2STR(mac_bytes));
    cJSON_AddStringToObject(macs, "eth", mac_str);

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
    cJSON_AddItemToObject(json, "ip", ips);
    cJSON_AddItemToObject(json, "mac", macs);
    cJSON_AddStringToObject(json, "hostname", hostname);
    cJSON_AddStringToObject(json, "compile_date", __DATE__);
    cJSON_AddStringToObject(json, "compile_time", __TIME__);
    cJSON_AddStringToObject(json, "git_version", GIT_VERSION);
    cJSON_AddBoolToObject  (json, "app_verified", (ota_state == ESP_OTA_IMG_VALID));
    cJSON_AddBoolToObject(json, "wireguard_up", wg_is_up());
    cJSON_AddNumberToObject(json, "spiffs_size", spiffs_total);
    cJSON_AddNumberToObject(json, "spiffs_free", spiffs_total - spiffs_used);
    #if defined(CONFIG_LOG_DEFAULT_LEVEL_NONE)
    cJSON_AddStringToObject(json, "log_level", "none");
    #elif defined(CONFIG_LOG_DEFAULT_LEVEL_ERROR)
    cJSON_AddStringToObject(json, "log_level", "error");
    #elif defined(CONFIG_LOG_DEFAULT_LEVEL_WARN)
    cJSON_AddStringToObject(json, "log_level", "warn");
    #elif defined(CONFIG_LOG_DEFAULT_LEVEL_INFO)
    cJSON_AddStringToObject(json, "log_level", "info");
    #elif defined(CONFIG_LOG_DEFAULT_LEVEL_DEBUG)
    cJSON_AddStringToObject(json, "log_level", "debug");
    #elif defined(CONFIG_LOG_DEFAULT_LEVEL_VERBOSE)
    cJSON_AddStringToObject(json, "log_level", "verbose");
    #else
    cJSON_AddStringToObject(json, "log_level", "unknown");
    #endif

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

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    cJSON_AddNumberToObject(json, "viewport_width_pixel", DISPLAY_VIEWPORT_WIDTH_PIXEL);
    cJSON_AddNumberToObject(json, "viewport_height_pixel", DISPLAY_VIEWPORT_HEIGHT_PIXEL);
    cJSON_AddNumberToObject(json, "frame_width_pixel", DISPLAY_FRAME_WIDTH_PIXEL);
    cJSON_AddNumberToObject(json, "frame_height_pixel", DISPLAY_FRAME_HEIGHT_PIXEL);
    cJSON_AddNumberToObject(json, "viewport_offset_x_pixel", DISPLAY_VIEWPORT_OFFSET_X_PIXEL);
    cJSON_AddNumberToObject(json, "viewport_offset_y_pixel", DISPLAY_VIEWPORT_OFFSET_Y_PIXEL);
#else
    cJSON_AddNullToObject(json, "viewport_width_pixel");
    cJSON_AddNullToObject(json, "viewport_height_pixel");
    cJSON_AddNullToObject(json, "frame_width_pixel");
    cJSON_AddNullToObject(json, "frame_height_pixel");
    cJSON_AddNullToObject(json, "viewport_offset_x_pixel");
    cJSON_AddNullToObject(json, "viewport_offset_y_pixel");
#endif

#if defined(DISPLAY_HAS_TEXT_BUFFER)
    cJSON_AddNumberToObject(json, "viewport_width_char", DISPLAY_VIEWPORT_WIDTH_CHAR);
    cJSON_AddNumberToObject(json, "viewport_height_char", DISPLAY_VIEWPORT_HEIGHT_CHAR);
    cJSON_AddNumberToObject(json, "frame_width_char", DISPLAY_FRAME_WIDTH_CHAR);
    cJSON_AddNumberToObject(json, "frame_height_char", DISPLAY_FRAME_HEIGHT_CHAR);
    cJSON_AddNumberToObject(json, "viewport_offset_x_char", DISPLAY_VIEWPORT_OFFSET_X_CHAR);
    cJSON_AddNumberToObject(json, "viewport_offset_y_char", DISPLAY_VIEWPORT_OFFSET_Y_CHAR);
#else
    cJSON_AddNullToObject(json, "viewport_width_char");
    cJSON_AddNullToObject(json, "viewport_height_char");
    cJSON_AddNullToObject(json, "frame_width_char");
    cJSON_AddNullToObject(json, "frame_height_char");
    cJSON_AddNullToObject(json, "viewport_offset_x_char");
    cJSON_AddNullToObject(json, "viewport_offset_y_char");
#endif

#if defined(CONFIG_DISPLAY_TYPE_SELECTION)
    cJSON_AddNumberToObject(json, "unitbuf_size", DISPLAY_UNIT_BUF_SIZE);
#else
    cJSON_AddNullToObject(json, "unitbuf_size");
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    cJSON_AddNumberToObject(json, "pixbuf_size", DISPLAY_PIX_BUF_SIZE);
    cJSON_AddStringToObject(json, "pixbuf_type", DISPLAY_PIX_BUF_TYPE);
#else
    cJSON_AddNullToObject(json, "pixbuf_size");
    cJSON_AddNullToObject(json, "pixbuf_type");
#endif

#if defined(DISPLAY_HAS_TEXT_BUFFER)
    cJSON_AddNumberToObject(json, "textbuf_size", DISPLAY_TEXT_BUF_SIZE);
    cJSON_AddNumberToObject(json, "charbuf_size", DISPLAY_CHAR_BUF_SIZE);
#else
    cJSON_AddNullToObject(json, "textbuf_size");
    cJSON_AddNullToObject(json, "charbuf_size");
#endif

    cJSON_AddBoolToObject(json, "brightness_control", DISPLAY_HAS_BRIGHTNESS_CONTROL);
#if defined(CONFIG_DISPLAY_TYPE_SELECTION)
    cJSON* sel_config;
    esp_err_t ret = display_selection_loadConfiguration(&httpd_nvs_handle, &sel_config, LOG_TAG);
    if (ret != ESP_OK) {
        cJSON_AddNullToObject(json, "config");
    } else {
        cJSON_AddItemToObject(json, "config", sel_config);
    }
#else
    cJSON_AddNullToObject(json, "config");
#endif

    cJSON* quirks_arr = cJSON_CreateArray();

    #if defined(CONFIG_DISPLAY_QUIRKS_COMBINING_FULL_STOP)
    cJSON* quirk_entry_combining_full_stop = cJSON_CreateString("combining_full_stop");
    cJSON_AddItemToArray(quirks_arr, quirk_entry_combining_full_stop);
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

httpd_handle_t httpd_init(nvs_handle_t* nvsHandle) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_nvs_handle = *nvsHandle;

    config.max_uri_handlers = 128;

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