#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include "wifi.h"


static uint16_t s_retry_num = 0;


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
}

void wifi_init(void) {
    // Init WiFi in STA mode, AP will be automatically used as fallback
    ESP_ERROR_CHECK(esp_netif_init());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_PROJ_STA_SSID,
            .password = CONFIG_PROJ_STA_PASS,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}