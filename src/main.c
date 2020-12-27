#include "esp_err.h"
#include "esp_log.h"
#include "mdns.h"
#include "nvs_flash.h"

#include "browser_ota.h"
#include "driver_display_flipdot.h"
#include "httpd.h"
#include "wifi.h"

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init();

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set(CONFIG_PROJ_HOSTNAME);
    mdns_instance_name_set(CONFIG_PROJ_HOSTNAME);

    httpd_handle_t server = start_webserver();
    browser_ota_init(&server);

    display_setupPeripherals();
}