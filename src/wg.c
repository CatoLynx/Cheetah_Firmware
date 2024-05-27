#include "wg.h"
#include "esp_log.h"
#include <string.h>


#define LOG_TAG "WireGuard"


#if defined(DISPLAY_HAS_CHAR_BUFFER)
extern uint8_t display_text_buffer[DISPLAY_TEXT_BUF_SIZE];
#endif


wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();
wireguard_ctx_t wg_ctx = {0};
bool wg_initialized = false;
bool wg_started = false;


esp_err_t wg_init(nvs_handle_t* nvsHandle) {
    esp_err_t ret = ESP_OK;
    ESP_LOGI(LOG_TAG, "Initializing");

    uint16_t listenPort;
    ret = nvs_get_u16(*nvsHandle, "wg_listen_port", &listenPort);
    if (ret != ESP_OK) return ESP_FAIL;

    uint16_t endpointPort;
    ret = nvs_get_u16(*nvsHandle, "wg_endpnt_port", &endpointPort);
    if (ret != ESP_OK) return ret;

    uint16_t keepalive;
    ret = nvs_get_u16(*nvsHandle, "wg_keepalive", &keepalive);
    if (ret != ESP_OK) return ret;

    char* privateKey = get_string_from_nvs(nvsHandle, "wg_private_key");
    if (privateKey == NULL || strlen(privateKey) == 0) {
        return ESP_FAIL;
    }

    char* publicKey = get_string_from_nvs(nvsHandle, "wg_public_key");
    if (publicKey == NULL || strlen(publicKey) == 0) {
        free(privateKey);
        return ESP_FAIL;
    }

    char* allowedIp = get_string_from_nvs(nvsHandle, "wg_allowed_ip");
    if (allowedIp == NULL || strlen(allowedIp) == 0) {
        free(privateKey);
        free(publicKey);
        return ESP_FAIL;
    }

    char* allowedIpMask = get_string_from_nvs(nvsHandle, "wg_allowed_mask");
    if (allowedIpMask == NULL || strlen(allowedIpMask) == 0) {
        free(privateKey);
        free(publicKey);
        free(allowedIp);
        return ESP_FAIL;
    }

    char* endpoint = get_string_from_nvs(nvsHandle, "wg_endpoint");
    if (endpoint == NULL || strlen(endpoint) == 0) {
        free(privateKey);
        free(publicKey);
        free(allowedIp);
        free(allowedIpMask);
        return ESP_FAIL;
    }

    wg_config.private_key = privateKey;
    wg_config.listen_port = listenPort;
    wg_config.public_key = publicKey;
    wg_config.allowed_ip = allowedIp;
    wg_config.allowed_ip_mask = allowedIpMask;
    wg_config.endpoint = endpoint;
    wg_config.port = endpointPort;
    wg_config.persistent_keepalive = keepalive;

    ret = esp_wireguard_init(&wg_config, &wg_ctx);
    if (ret == ESP_OK) {
        wg_initialized = true;
        ESP_LOGI(LOG_TAG, "Initialized");
    }
    return ret;
}

esp_err_t wg_start() {
    if (!wg_initialized) return ESP_FAIL;
    ESP_LOGI(LOG_TAG, "Connecting");
    if (wg_started) {
        #if defined(DISPLAY_HAS_CHAR_BUFFER)
        //display_text_buffer[0] = 'W';
        #endif
        ESP_LOGI(LOG_TAG, "Already connected!");
        return ESP_OK;
    }
    esp_err_t ret = esp_wireguard_connect(&wg_ctx);
    if (ret == ESP_OK) {
        #if defined(DISPLAY_HAS_CHAR_BUFFER)
        //display_text_buffer[0] = 'W';
        #endif
        ESP_LOGI(LOG_TAG, "Connected");
        wg_started = true;
    }
    return ret;
}

bool wg_is_up() {
    if (!wg_initialized) return false;
    if (!wg_started) return false;
    return (esp_wireguardif_peer_is_up(&wg_ctx) == ESP_OK);
}