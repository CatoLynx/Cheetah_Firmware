#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"

#include "wifi.h"
#include "ntp.h"
#include "wg.h"

#define LOG_TAG "WiFi"


static uint16_t s_retry_num = 0;
static uint8_t sta_retries = 0;
static uint8_t sta_phase2 = 0;
static uint8_t sta_phase2_ttls = 0;
static size_t sta_ssid_len = 33;
static size_t sta_anon_ident_len = 65;
static size_t sta_ident_len = 65;
static size_t sta_pass_len = 65;
static size_t sta_fallb_ssid_len = 33;
static size_t sta_fallb_pass_len = 65;
static size_t ap_ssid_len = 33;
static size_t ap_pass_len = 65;
static char sta_ssid[33];
static char sta_anon_ident[65];
static char sta_ident[65];
static char sta_pass[65];
static char sta_fallb_ssid[33];
static char sta_fallb_pass[65];
static char ap_ssid[33];
static char ap_pass[65];
static uint16_t ap_timeout = 0;
static int64_t ap_start_time = 0;
static uint8_t ap_client_connected = 0;
uint8_t wifi_gotIP = 0;
esp_netif_t* netif_wifi_sta = NULL;
esp_netif_t* netif_wifi_ap = NULL;

extern char hostname[63];

#if defined(DISPLAY_HAS_TEXT_BUFFER)
extern uint8_t display_text_buffer[DISPLAY_TEXT_BUF_SIZE];
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
                wg_update_interfaces();
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
                ap_client_connected = 1;
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

                ESP_LOGI(LOG_TAG, "Got IP");
                ESP_LOGI(LOG_TAG, "IP:      " IPSTR, IP2STR(&event->ip_info.ip));
                ESP_LOGI(LOG_TAG, "Mask:    " IPSTR, IP2STR(&event->ip_info.netmask));
                ESP_LOGI(LOG_TAG, "Gateway: " IPSTR, IP2STR(&event->ip_info.gw));

                #if defined(DISPLAY_HAS_TEXT_BUFFER) && defined(CONFIG_DISPLAY_SHOW_MESSAGES)
                char temp[19];
                sprintf(temp, IPSTR, IP2STR(&event->ip_info.ip));
                STRCPY_TEXTBUF((char*)display_text_buffer, temp, DISPLAY_TEXT_BUF_SIZE);
                #endif
                s_retry_num = 0;
                wg_update_interfaces();
                if (!ntp_is_synced()) ntp_sync_time();
                break;
            }
        }
    }
}

void wifi_init_ap(void) {
    netif_wifi_ap = esp_netif_create_default_wifi_ap();

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
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ap_start_time = esp_timer_get_time();

    ESP_LOGI(LOG_TAG, "AP started. SSID: %s, password: %s", ap_ssid, ap_pass);
    #if defined(CONFIG_DISPLAY_SHOW_MESSAGES)
    STRCPY_TEXTBUF((char*)display_text_buffer, "AP MODE", DISPLAY_TEXT_BUF_SIZE);
    #endif
}

void wifi_init(nvs_handle_t* nvsHandle) {
    // Read STA and AP SSID and password from NVS
    esp_err_t ret;
    uint8_t sta_enterprise = 1;
    uint8_t sta_credentials_valid = 1;
    uint8_t sta_fallback_credentials_valid = 1;
    memset(sta_ssid, 0x00, sta_ssid_len);
    memset(sta_anon_ident, 0x00, sta_anon_ident_len);
    memset(sta_ident, 0x00, sta_ident_len);
    memset(sta_pass, 0x00, sta_pass_len);
    memset(sta_fallb_ssid, 0x00, sta_fallb_ssid_len);
    memset(sta_fallb_pass, 0x00, sta_fallb_pass_len);
    memset(ap_ssid, 0x00, ap_ssid_len);
    memset(ap_pass, 0x00, ap_pass_len);
    ret = nvs_get_u16(*nvsHandle, "ap_timeout", &ap_timeout);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ap_timeout = 0;
    } else {
        ESP_ERROR_CHECK(ret);
        // If not 0, force at least 10 seconds timeout to be safe
        if (ap_timeout != 0 && ap_timeout < 10) ap_timeout = 10;
    }
    ret = nvs_get_str(*nvsHandle, "sta_ssid", sta_ssid, &sta_ssid_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_str(*nvsHandle, "sta_ident", sta_ident, &sta_ident_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_enterprise = 0;
    } else {
        ESP_ERROR_CHECK(ret);
        if (strlen(sta_ident) == 0) sta_enterprise = 0;
    }
    ret = nvs_get_str(*nvsHandle, "sta_anon_ident", sta_anon_ident, &sta_anon_ident_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        strncpy((char*)sta_anon_ident, sta_ident, sta_ident_len - 1);
    } else {
        ESP_ERROR_CHECK(ret);
        if (strlen(sta_anon_ident) == 0) {
            strncpy((char*)sta_anon_ident, sta_ident, sta_ident_len - 1);
        }
    }
    ret = nvs_get_str(*nvsHandle, "sta_pass", sta_pass, &sta_pass_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    if (strlen(sta_ssid) == 0) sta_credentials_valid = 0;
    ret = nvs_get_u8(*nvsHandle, "sta_retries", &sta_retries);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_retries = 5;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_u8(*nvsHandle, "sta_phase2", &sta_phase2);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_enterprise = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_u8(*nvsHandle, "sta_phase2_ttls", &sta_phase2_ttls);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_phase2_ttls = WPA2E_PH2_TTLS_NONE;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_str(*nvsHandle, "sta_fallb_ssid", sta_fallb_ssid, &sta_fallb_ssid_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_fallback_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }
    ret = nvs_get_str(*nvsHandle, "sta_fallb_pass", sta_fallb_pass, &sta_fallb_pass_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        sta_fallback_credentials_valid = 0;
    } else {
        ESP_ERROR_CHECK(ret);
    }

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

    // Init WiFi in STA mode, STA fallback will be automatically used,
    // AP as last resort
    netif_wifi_sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    // If no valid credentials are stored, use fallback STA
    if (!sta_credentials_valid) {
        // If no valid fallback credentials are stored either, go straight to AP mode
        if (!sta_fallback_credentials_valid) {
            wifi_init_ap();
        }
        return;
    }

    wifi_config_t wifi_config;
    // Check if we're using main or fallback STA
    if (sta_credentials_valid) {
        // Main STA
        ESP_LOGI(LOG_TAG, "Using main STA WiFi");
        if (strlen(sta_pass) != 0) {
            wifi_config = (wifi_config_t){
                .sta = {
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                    * However these modes are deprecated and not advisable to be used. In case your access point
                    * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = sta_enterprise ? WIFI_AUTH_WPA2_ENTERPRISE : WIFI_AUTH_WPA2_PSK,

                    .pmf_cfg = {
                        .capable = false,
                        .required = false
                    },
                },
            };
            sta_ssid[sta_ssid_len - 1] = 0x00;
            sta_pass[sta_pass_len - 1] = 0x00;
            strncpy((char*)wifi_config.sta.ssid, sta_ssid, sta_ssid_len - 1);
            if (!sta_enterprise) strncpy((char*)wifi_config.sta.password, sta_pass, sta_pass_len - 1);

            // If the parameters for WPA2 Enterprise are set, set corresponsding parameters
            if (sta_enterprise) {
                esp_eap_client_set_identity((uint8_t*)sta_anon_ident, strlen(sta_anon_ident));

                switch (sta_phase2) {
                    case WPA2E_PH2_TLS: {
                        ESP_LOGE(LOG_TAG, "TLS as EAP phase 2 method is not supported yet!");
                        return;
                    }

                    case WPA2E_PH2_PEAP: {
                        ESP_ERROR_CHECK(esp_eap_client_set_username((uint8_t*)sta_ident, strlen(sta_ident)));
                        ESP_ERROR_CHECK(esp_eap_client_set_password((uint8_t*)sta_pass, strlen(sta_pass)));
                        break;
                    }

                    case WPA2E_PH2_TTLS: {
                        ESP_ERROR_CHECK(esp_eap_client_set_username((uint8_t*)sta_ident, strlen(sta_ident)));
                        ESP_ERROR_CHECK(esp_eap_client_set_password((uint8_t*)sta_pass, strlen(sta_pass)));
                        ESP_ERROR_CHECK(esp_eap_client_set_ttls_phase2_method((esp_eap_ttls_phase2_types)sta_phase2_ttls));
                        break;
                    }
                }
            }
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
    } else {
        // Fallback STA
        ESP_LOGI(LOG_TAG, "Using fallback STA WiFi");
        if (strlen(sta_fallb_pass) != 0) {
            wifi_config = (wifi_config_t){
                .sta = {
                    /* Setting a password implies station will connect to all security modes including WEP/WPA.
                    * However these modes are deprecated and not advisable to be used. In case your access point
                    * doesn't support WPA2, these mode can be enabled by commenting below line */
                    .threshold.authmode = WIFI_AUTH_WPA2_PSK,

                    .pmf_cfg = {
                        .capable = false,
                        .required = false
                    },
                },
            };
            sta_fallb_ssid[sta_fallb_ssid_len - 1] = 0x00;
            sta_fallb_pass[sta_fallb_pass_len - 1] = 0x00;
            strncpy((char*)wifi_config.sta.ssid, sta_fallb_ssid, sta_fallb_ssid_len - 1);
            strncpy((char*)wifi_config.sta.password, sta_fallb_pass, sta_fallb_pass_len - 1);
        } else {
            wifi_config = (wifi_config_t){
                .sta = {
                    .pmf_cfg = {
                        .capable = false,
                        .required = false
                    },
                },
            };
            sta_fallb_ssid[sta_fallb_ssid_len - 1] = 0x00;
            strncpy((char*)wifi_config.sta.ssid, sta_fallb_ssid, sta_fallb_ssid_len - 1);
        }
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    if (sta_enterprise && sta_credentials_valid) ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
    ESP_ERROR_CHECK(esp_wifi_start());
    if (netif_wifi_sta != NULL) ESP_ERROR_CHECK(esp_netif_set_hostname(netif_wifi_sta, hostname));

    #if defined(CONFIG_DISPLAY_SHOW_MESSAGES)
    STRCPY_TEXTBUF((char*)display_text_buffer, "CONNECTING", DISPLAY_TEXT_BUF_SIZE);
    #endif
}

void wifi_stop(void) {
    wifi_mode_t mode;
    ESP_ERROR_CHECK(esp_wifi_get_mode(&mode));
    if (mode == WIFI_MODE_AP) {
        ap_start_time = 0;
        ap_client_connected = 0;
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
}

void wifi_timeout_task(void* arg) {
    while (1) {
        if (   !ap_client_connected /* If a client was connected at any point, timeout is disabled */
            && ap_timeout != 0      /* 0 = Timeout disabled */
            && ap_start_time != 0   /* If AP hasn't been started, continue */) {
            int64_t now = esp_timer_get_time();
            if (now - ap_start_time >= ap_timeout * 1000000LL) {
                ESP_LOGI(LOG_TAG, "AP timed out, disabling");
                wifi_stop();
                #if defined(CONFIG_DISPLAY_SHOW_MESSAGES)
                STRCPY_TEXTBUF((char*)display_text_buffer, "AP TIMEOUT", DISPLAY_TEXT_BUF_SIZE);
                #endif
            }
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}