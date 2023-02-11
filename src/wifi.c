#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "macros.h"

#include "wifi.h"
#include "ntp.h"

#define LOG_TAG "WiFi"


static uint16_t s_retry_num = 0;
static uint8_t sta_retries = 0;
static size_t sta_ssid_len = 33;
static size_t sta_pass_len = 65;
static size_t ap_ssid_len = 33;
static size_t ap_pass_len = 65;
static char sta_ssid[33];
static char sta_pass[65];
static char ap_ssid[33];
static char ap_pass[65];
uint8_t wifi_gotIP = 0;

extern char hostname[63];

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
                wifi_gotIP = 0;
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                ESP_LOGI(LOG_TAG, "Disconnected from %s", event->ssid);
                if (s_retry_num < sta_retries) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(LOG_TAG, "Reconnecting to %s, attempt %d of %d", event->ssid, s_retry_num, sta_retries);
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
                wifi_gotIP = 1;
                ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
                ESP_LOGI(LOG_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
                #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
                char temp[19];
                sprintf(temp, "IP=" IPSTR, IP2STR(&event->ip_info.ip));
                strncpy((char*)display_char_buffer, temp, DISPLAY_CHARBUF_SIZE);
                #endif
                s_retry_num = 0;
                ntp_sync_time();
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
            .ssid_len = strlen(ap_ssid),
            .max_connection = 3,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    ap_ssid[ap_ssid_len - 1] = 0x00;
    ap_pass[ap_pass_len - 1] = 0x00;
    strncpy((char*)wifi_config.ap.ssid, ap_ssid, ap_ssid_len - 1);
    strncpy((char*)wifi_config.ap.password, ap_pass, ap_pass_len - 1);
    if (strlen(ap_pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(LOG_TAG, "AP started. SSID: %s, password: %s", ap_ssid, ap_pass);
    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    strncpy((char*)display_char_buffer, "AP MODE", DISPLAY_CHARBUF_SIZE);
    #endif
}

void wifi_init(nvs_handle_t* nvsHandle) {
    // Read STA and AP SSID and password from NVS
    esp_err_t ret;
    uint8_t sta_credentials_valid = 1;
    memset(sta_ssid, 0x00, sta_ssid_len);
    memset(sta_pass, 0x00, sta_pass_len);
    memset(ap_ssid, 0x00, ap_ssid_len);
    memset(ap_pass, 0x00, ap_pass_len);
    ret = nvs_get_str(*nvsHandle, "sta_ssid", sta_ssid, &sta_ssid_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_str(*nvsHandle, "sta_pass", sta_pass, &sta_pass_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    if (strlen(sta_ssid) == 0) sta_credentials_valid = 0;
    ret = nvs_get_u8(*nvsHandle, "sta_retries", &sta_retries);

    ESP_LOGI(LOG_TAG, "Getting AP SSID from NVS");
    ret = nvs_get_str(*nvsHandle, "ap_ssid", ap_ssid, &ap_ssid_len);
    ap_ssid_len = 33; // Reset after nvs_get_str modified it
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(LOG_TAG, "AP SSID not found in NVS");
        ap_ssid[ap_ssid_len - 1] = 0x00;
        strncpy(ap_ssid, CONFIG_PROJ_DEFAULT_AP_SSID, ap_ssid_len - 1);
        ESP_LOGI(LOG_TAG, "Fallback: %s", ap_ssid);
    } else {
        if (strlen(ap_ssid) == 0) {
            ap_ssid[ap_ssid_len - 1] = 0x00;
            strncpy(ap_ssid, CONFIG_PROJ_DEFAULT_AP_SSID, ap_ssid_len - 1);
            ESP_LOGI(LOG_TAG, "AP SSID strlen is 0, Fallback: %s", ap_ssid);
        }
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_str(*nvsHandle, "ap_pass", ap_pass, &ap_pass_len);
    ap_pass_len = 65; // Reset after nvs_get_str modified it
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ap_pass[ap_pass_len - 1] = 0x00;
        strncpy(ap_pass, CONFIG_PROJ_DEFAULT_AP_PASS, ap_pass_len - 1);
    } else {
        if (strlen(ap_pass) == 0) {
            ap_pass[ap_pass_len - 1] = 0x00;
            strncpy(ap_pass, CONFIG_PROJ_DEFAULT_AP_PASS, ap_pass_len - 1);
        }
        ESP_ERROR_CHECK(ret);
    }

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
        sta_ssid[sta_ssid_len - 1] = 0x00;
        sta_pass[sta_pass_len - 1] = 0x00;
        strncpy((char*)wifi_config.sta.ssid, sta_ssid, sta_ssid_len - 1);
        strncpy((char*)wifi_config.sta.password, sta_pass, sta_pass_len - 1);
    } else {
        wifi_config = (wifi_config_t){
            .sta = {
                .pmf_cfg = {
                    .capable = false,
                    .required = false
                },
            },
        };
        sta_ssid[sta_ssid_len - 1] = 0x00;
        strncpy((char*)wifi_config.sta.ssid, sta_ssid, sta_ssid_len - 1);
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_STA, hostname));

    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    strncpy((char*)display_char_buffer, "CONNECTING", DISPLAY_CHARBUF_SIZE);
    #endif
}