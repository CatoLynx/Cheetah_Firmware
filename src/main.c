#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs_flash.h"

#include "browser_ota.h"
#include "driver_display_flipdot.h"
#include "httpd.h"
#include "macros.h"
#include "tpm2net.h"
#include "wifi.h"


uint8_t display_output_buffer[DISPLAY_FRAMEBUF_SIZE] = {0};
uint8_t temp_output_buffer[DISPLAY_FRAMEBUF_SIZE] = {0};
uint8_t prev_display_output_buffer[DISPLAY_FRAMEBUF_SIZE] = {0};


static void display_refresh_task(void* arg) {
    while (1) {
        memcpy(temp_output_buffer, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        display_renderFrame8bpp(temp_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
    }
}


void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set(CONFIG_PROJ_HOSTNAME);
    mdns_instance_name_set(CONFIG_PROJ_HOSTNAME);

    httpd_handle_t server = start_webserver();
    browser_ota_init(&server);
    display_setupPeripherals();
    tpm2net_start(display_output_buffer);

    xTaskCreate(display_refresh_task, "display_refresh", 4096, NULL, 5, NULL);
}