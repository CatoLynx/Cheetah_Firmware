#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "macros.h"

#include "wifi.h"
#include "settings_secret.h"

#define LOG_TAG "WiFi"

// TODO: Use sta_retries from VNS instead of CONFIG_PROJ_STA_MAX_RECONNECTS


static uint16_t s_retry_num = 0;

#if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
extern uint8_t display_char_buffer[DISPLAY_CHARBUF_SIZE];
#endif


static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if(event_base == WIFI_EVENT) {
        switch(event_id) {
            case WIFI_EVENT_STA_START: {
                esp_wifi_connect();
                ESP_LOGI(LOG_TAG, "STA Start");
                break;
            }
            
            case WIFI_EVENT_STA_CONNECTED: {
                wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*) event_data;
                ESP_LOGI(LOG_TAG, "Connected to %s", event->ssid);
                s_retry_num = 0;
                break;
            }
            
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGI(LOG_TAG, "Disconnected from %s", event->ssid);
                if (s_retry_num < CONFIG_PROJ_STA_MAX_RECONNECTS) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(LOG_TAG, "Reconnecting to %s, attempt %d of %d", event->ssid, s_retry_num, CONFIG_PROJ_STA_MAX_RECONNECTS);
                } else {
                    wifi_init_ap();
                }
                break;
            }

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(LOG_TAG, "Device "MACSTR" connected, AID %d",
                        MAC2STR(event->mac), event->aid);
                break;
            }
            
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(LOG_TAG, "Device "MACSTR" disconnected, AID %d",
                        MAC2STR(event->mac), event->aid);
                break;
            }
        }
    } else if (event_base == IP_EVENT) {
        switch(event_id) {
            case IP_EVENT_STA_GOT_IP: {
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(LOG_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
                char temp[19];
                sprintf(temp, "IP=" IPSTR, IP2STR(&event->ip_info.ip));
                strncpy((char*)display_char_buffer, temp, DISPLAY_CHARBUF_SIZE);
                #endif
                s_retry_num = 0;
                break;
            }
        }
    }
}

void wifi_init_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_PROJ_AP_SSID,
            .ssid_len = strlen(CONFIG_PROJ_AP_SSID),
            .password = CONFIG_PROJ_AP_PASS,
            .max_connection = 3,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(CONFIG_PROJ_AP_SSID) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(LOG_TAG, "AP started. SSID: %s, password: %s",
             CONFIG_PROJ_AP_SSID, CONFIG_PROJ_AP_PASS);
    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    strncpy((char*)display_char_buffer, "AP MODE", DISPLAY_CHARBUF_SIZE);
    #endif
}

void wifi_init(nvs_handle_t* nvsHandle) {
    // Read STA SSID and password from NVS
    esp_err_t ret;
    uint8_t sta_credentials_valid = 1;
    size_t ssid_len = 33;
    size_t pass_len = 65;
    char sta_ssid[33];
    char sta_pass[65];
    memset(sta_ssid, 0x00, ssid_len);
    memset(sta_pass, 0x00, pass_len);
    ret = nvs_get_str(*nvsHandle, "sta_ssid", sta_ssid, &ssid_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_str(*nvsHandle, "sta_pass", sta_pass, &pass_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    if (strlen(sta_ssid) == 0) sta_credentials_valid = 0;

    // Init WiFi in STA mode, AP will be automatically used as fallback
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // If no valid credentials are stored, go straight to AP mode
    if (!sta_credentials_valid) {
        wifi_init_ap();
        return;
    }

    wifi_config_t wifi_config;
    if (strlen(sta_pass) != 0) {
        wifi_config = (wifi_config_t){
            .sta = {
                /* Setting a password implies station will connect to all security modes including WEP/WPA.
                * However these modes are deprecated and not advisable to be used. Incase your Access point
                * doesn't support WPA2, these mode can be enabled by commenting below line */
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,

                .pmf_cfg = {
                    .capable = false,
                    .required = false
                },
            },
        };
        memcpy(wifi_config.sta.ssid, sta_ssid, ssid_len);
        memcpy(wifi_config.sta.password, sta_pass, pass_len);
    } else {
        wifi_config = (wifi_config_t){
            .sta = {
                .pmf_cfg = {
                    .capable = false,
                    .required = false
                },
            },
        };
        memcpy(wifi_config.sta.ssid, sta_ssid, ssid_len);
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    strncpy((char*)display_char_buffer, "CONNECTING", DISPLAY_CHARBUF_SIZE);
    #endif
}