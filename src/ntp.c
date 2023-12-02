#include "ntp.h"
#include "wg.h"
#include "esp_log.h"
#include "macros.h"
#include <string.h>


#define LOG_TAG "NTP"


#if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
extern uint8_t display_text_buffer[DISPLAY_TEXTBUF_SIZE];
#endif

bool ntp_initialized = false;
bool ntp_started = false;


void ntp_sync_cb(struct timeval *tv) {
    ESP_LOGI(LOG_TAG, "NTP time synced");
    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    //display_text_buffer[0] = 'N';
    #endif
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    wg_start();
}

void ntp_init(void) {
    if (ntp_initialized) return;
    ESP_LOGI(LOG_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(ntp_sync_cb);
    sntp_init();
    ntp_initialized = true;
}

void ntp_sync_time(void) {
    if (!ntp_initialized) ntp_init();

    int retry = 0;
    const int retry_count = 20;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(LOG_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}