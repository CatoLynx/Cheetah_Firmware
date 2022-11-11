#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_ota_ops.h"

#include "artnet.h"
#include "browser_canvas.h"
#include "browser_config.h"
#include "browser_ota.h"
#include "httpd.h"
#include "logging_tcp.h"
#include "macros.h"
#include "telegram_bot.h"
#include "tpm2net.h"
#include "ethernet.h"
#include "wifi.h"
#include "util_fan.h"
#include "util_gpio.h"

#if defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_LAWO_ALUMA)
#include "driver_display_flipdot_lawo_aluma.h"
#elif defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_BROSE)
#include "driver_display_flipdot_brose.h"
#elif defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_SAFLAP)
#include "driver_display_flipdot_aesco_saflap.h"
#elif defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER) || defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER_I2S)
#include "driver_display_led_shift_register.h"
#elif defined(CONFIG_DISPLAY_DRIVER_LED_AESYS_I2S)
#include "driver_display_led_aesys.h"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_SEG_LCD_SPI)
#include "driver_display_char_segment_lcd_spi.h"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_SPI)
#include "driver_display_char_16seg_led_spi.h"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_KRONE_9000)
#include "driver_display_char_krone_9000.h"
#endif

#if defined(CONFIG_DISPLAY_TYPE_PIXEL)
#if defined(CONFIG_TPM2NET_FRAME_TYPE_1BPP)
#define TPM2NET_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_1BPP
#elif defined(CONFIG_TPM2NET_FRAME_TYPE_8BPP)
#define TPM2NET_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_8BPP
#elif defined(CONFIG_TPM2NET_FRAME_TYPE_24BPP)
#define TPM2NET_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_24BPP
#endif
uint8_t tpm2net_output_buffer[TPM2NET_FRAMEBUF_SIZE] = {0};

#if defined(CONFIG_ARTNET_FRAME_TYPE_1BPP)
#define ARTNET_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_1BPP
#elif defined(CONFIG_ARTNET_FRAME_TYPE_8BPP)
#define ARTNET_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_8BPP
#elif defined(CONFIG_ARTNET_FRAME_TYPE_24BPP)
#define ARTNET_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_24BPP
#endif
uint8_t artnet_output_buffer[ARTNET_FRAMEBUF_SIZE] = {0};
#endif

#if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
uint8_t display_char_buffer[DISPLAY_CHARBUF_SIZE] = {0};
#endif

uint8_t display_output_buffer[DISPLAY_FRAMEBUF_SIZE] = {0};

#if defined(CONFIG_DISPLAY_TYPE_PIXEL)
uint8_t temp_output_buffer[DISPLAY_FRAMEBUF_SIZE] = {0};
#endif

#if defined(CONFIG_DISPLAY_USE_PREV_FRAMEBUF)
    uint8_t prev_display_output_buffer[DISPLAY_FRAMEBUF_SIZE] = {0};
#else
    uint8_t* prev_display_output_buffer = NULL;
#endif

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
uint8_t display_prevBrightness = 0;
uint8_t display_brightness = 255;
#endif

size_t hostname_length = 64;
char hostname[64];


static void display_refresh_task(void* arg) {
    while (1) {
        #if defined(CONFIG_DISPLAY_TYPE_PIXEL)
            // Framebuffer format: Top-to-bottom columns
            memcpy(temp_output_buffer, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
            #if defined(CONFIG_DISPLAY_FRAME_TYPE_1BPP)
                display_render_frame_1bpp(temp_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
            #elif defined(CONFIG_DISPLAY_FRAME_TYPE_8BPP)
                display_render_frame_8bpp(temp_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
            #elif defined(CONFIG_DISPLAY_FRAME_TYPE_24BPP)
                display_render_frame_24bpp(temp_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
            #endif
        #elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
        display_charbuf_to_framebuf(display_char_buffer, display_output_buffer, DISPLAY_CHARBUF_SIZE, DISPLAY_FRAMEBUF_SIZE);
        display_render_frame(display_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        #endif

        #if defined(CONFIG_FAN_ENABLED)
        fan_set_target_speed(display_get_fan_speed(display_output_buffer, DISPLAY_FRAMEBUF_SIZE));
        #endif

        #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
        if (display_brightness != display_prevBrightness) {
            display_set_brightness(display_brightness);
            display_prevBrightness = display_brightness;
        }
        #endif

        taskYIELD();
        vTaskDelay(1);
    }
}


void app_main(void) {
    #if defined(CONFIG_TCP_LOG_ENABLED)
    tcp_log_init();
    #endif

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open("cheetah", NVS_READWRITE, &nvs_handle);
    ESP_ERROR_CHECK(ret);

    ret = nvs_get_str(nvs_handle, "hostname", hostname, &hostname_length);
    hostname_length = 64; // Reset after nvs_get_str modified it
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        hostname[hostname_length - 1] = 0x00;
        strncpy(hostname, CONFIG_PROJ_DEFAULT_HOSTNAME, hostname_length - 1);
    } else {
        ESP_ERROR_CHECK(ret);
    }
    if (strlen(hostname) == 0) {
        hostname[hostname_length - 1] = 0x00;
        strncpy(hostname, CONFIG_PROJ_DEFAULT_HOSTNAME, hostname_length - 1);
    }

    #if defined(CONFIG_FAN_ENABLED)
    ESP_ERROR_CHECK(fan_init());
    #endif

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init(&nvs_handle);

    #if defined(CONFIG_TCP_LOG_ENABLED)
    tcp_log_start();
    #endif

    #if defined(CONFIG_ETHERNET_ENABLED)
    ethernet_init();
    #endif

    ESP_ERROR_CHECK(mdns_init());
    mdns_hostname_set(hostname);
    mdns_instance_name_set(hostname);

    httpd_handle_t server = httpd_init();
    browser_ota_init(&server);
    browser_config_init(&server, &nvs_handle);

    display_init();

    #if defined(CONFIG_DISPLAY_TYPE_PIXEL)
    tpm2net_init(display_output_buffer, tpm2net_output_buffer, DISPLAY_FRAMEBUF_SIZE, TPM2NET_FRAMEBUF_SIZE);
    artnet_init(display_output_buffer, artnet_output_buffer, DISPLAY_FRAMEBUF_SIZE, ARTNET_FRAMEBUF_SIZE);
    browser_canvas_init(&server, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
    #endif
    
    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    browser_canvas_init(&server, display_char_buffer, DISPLAY_CHARBUF_SIZE);
    telegram_bot_init(&nvs_handle, display_char_buffer, DISPLAY_CHARBUF_SIZE);
    #endif

    #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
    browser_canvas_register_brightness(&server, &display_brightness);
    #endif

    xTaskCreate(display_refresh_task, "display_refresh", 4096, NULL, 5, NULL);

    #if defined(CONFIG_FAN_ENABLED)
    xTaskCreate(fan_speed_adjust_task, "fan_speed_adjust", 4096, NULL, 3, NULL);
    #endif
}