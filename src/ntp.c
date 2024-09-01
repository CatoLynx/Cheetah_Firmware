#include "ntp.h"
#include "esp_log.h"
#include <string.h>


#define LOG_TAG "NTP"


#if defined(DISPLAY_HAS_TEXT_BUFFER)
extern uint8_t display_text_buffer[DISPLAY_TEXT_BUF_SIZE];
#endif

static bool ntp_initialized = false;
static bool ntp_sync_in_progress = false;
static bool ntp_synced = false;


void ntp_sync_cb(struct timeval *tv) {
    ntp_sync_in_progress = false;
    ntp_synced = true;

    // Print time
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;
    time(&now);
    // Europe/Berlin
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(LOG_TAG, "Time synced: %s", strftime_buf);
    
    #if defined(DISPLAY_HAS_TEXT_BUFFER)
    //display_text_buffer[0] = 'N';
    #endif
}

void ntp_init(void) {
    if (ntp_initialized) return;
    ESP_LOGI(LOG_TAG, "Initializing SNTP");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(ntp_sync_cb);
    esp_sntp_init();
    ntp_initialized = true;
}

void ntp_sync_time(void) {
    if (ntp_sync_in_progress) {
        ESP_LOGI(LOG_TAG, "NTP sync already in progress");
        return;
    }

    ntp_sync_in_progress = true;
    if (!ntp_initialized) ntp_init();
    int retry = 0;
    const int retry_count = 20;
    while (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(LOG_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    ntp_sync_in_progress = false;
}

bool ntp_is_synced(void) {
    return ntp_synced;
}