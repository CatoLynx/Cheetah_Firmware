#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mdns.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "esp_spiffs.h"
#include "cJSON.h"

#include "config_global.h"
#include "artnet.h"
#include "browser_canvas.h"
#include "browser_config.h"
#include "browser_spiffs.h"
#include "browser_ota.h"
#include "git_version.h"
#include "httpd.h"
#include "logging_tcp.h"
#include "macros.h"
#include "remote_poll.h"
#include "telegram_bot.h"
#include "tpm2net.h"
#include "ethernet.h"
#include "wifi.h"
#include "util_buffer.h"
#include "util_fan.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "wg.h"


#define LOG_TAG "MAIN"


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
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X)
#include "driver_display_char_16seg_led_ws281x.h"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_9000)
#include "driver_display_sel_krone_9000.h"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_8200_PST)
#include "driver_display_sel_krone_8200_pst.h"
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
uint16_t display_quirk_flags_buffer[DISPLAY_CHARBUF_SIZE] = {0};
uint8_t display_text_buffer[DISPLAY_TEXTBUF_SIZE] = {0};
#endif

#if defined(CONFIG_DISPLAY_TYPE_SELECTION)
// For selection displays, use an additional mask to determine which of the units
// are present. This is necessary since configuration is done dynamically
// using a JSON file in SPIFFS. This mask buffer conatins a 1 bit
// if the unit at the given address is present, otherwise a 0 bit.
uint8_t display_framebuf_mask[DIV_CEIL(DISPLAY_FRAMEBUF_SIZE, 8)] = {0};
uint16_t display_num_units = 0;
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

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
cJSON* display_prevShader = NULL;
cJSON* display_shader = NULL;
#endif

size_t hostname_length = 64;
char hostname[64];


static void display_refresh_task(void* arg) {
    // Start with splashscreen
    #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    char hostname_upper[64];
    strncpy(hostname_upper, hostname, 63);
    str_toUpper(hostname_upper);

    char splash_text[74];

    #if defined(GIT_VERSION)
    snprintf(splash_text, 74, "%s %s", hostname_upper, GIT_VERSION);
    #else
    snprintf(splash_text, 74, "%s", hostname_upper);
    #endif

    STRCPY_TEXTBUF((char*)display_text_buffer, splash_text, DISPLAY_TEXTBUF_SIZE);

    buffer_textbuf_to_charbuf(display_text_buffer, display_char_buffer, display_quirk_flags_buffer, DISPLAY_TEXTBUF_SIZE, DISPLAY_CHARBUF_SIZE);
    display_charbuf_to_framebuf(display_char_buffer, display_quirk_flags_buffer, display_output_buffer, DISPLAY_CHARBUF_SIZE, DISPLAY_FRAMEBUF_SIZE);
    display_render_frame(display_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);

    taskYIELD();
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    #endif

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
        buffer_textbuf_to_charbuf(display_text_buffer, display_char_buffer, display_quirk_flags_buffer, DISPLAY_TEXTBUF_SIZE, DISPLAY_CHARBUF_SIZE);
        display_charbuf_to_framebuf(display_char_buffer, display_quirk_flags_buffer, display_output_buffer, DISPLAY_CHARBUF_SIZE, DISPLAY_FRAMEBUF_SIZE);
        display_render_frame(display_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        #elif defined(CONFIG_DISPLAY_TYPE_SELECTION)
        display_render_frame(display_output_buffer, prev_display_output_buffer, DISPLAY_FRAMEBUF_SIZE, display_framebuf_mask, display_num_units);
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

        #if defined(CONFIG_DISPLAY_HAS_SHADERS)
        if (display_shader != display_prevShader) {
            display_set_shader(display_shader);
            
            // Delete previous shader data if necessary
            if (display_prevShader != NULL) {
                cJSON_Delete(display_prevShader);
            }
            display_prevShader = display_shader;
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

    esp_vfs_spiffs_conf_t spiffs_config = {
      .base_path = "/spiffs",
      .partition_label = SPIFFS_PARTITION_LABEL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    ESP_LOGI(LOG_TAG, "Initializing SPIFFS");
    ret = esp_vfs_spiffs_register(&spiffs_config);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(LOG_TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(LOG_TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(LOG_TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(spiffs_config.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(LOG_TAG, "Partition size: total: %d, used: %d", total, used);
    }

    #if defined(CONFIG_FAN_ENABLED)
    ESP_ERROR_CHECK(fan_init());
    #endif

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wg_init(&nvs_handle); // Init WireGuard before WiFi to ensure it's ready when WiFI gets an IP
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

    httpd_handle_t server = httpd_init(&nvs_handle);
    browser_ota_init(&server);
    browser_config_init(&server, &nvs_handle);
    browser_spiffs_init(&server);

    #if defined(CONFIG_DISPLAY_TYPE_SELECTION)
    ret = display_init(&nvs_handle, display_framebuf_mask, &display_num_units);
    #else
    ret = display_init(&nvs_handle);
    #endif
    if (ret == ESP_OK) {
        #if defined(CONFIG_DISPLAY_TYPE_PIXEL)
        tpm2net_init(display_output_buffer, tpm2net_output_buffer, DISPLAY_FRAMEBUF_SIZE, TPM2NET_FRAMEBUF_SIZE);
        artnet_init(display_output_buffer, artnet_output_buffer, DISPLAY_FRAMEBUF_SIZE, ARTNET_FRAMEBUF_SIZE);
        browser_canvas_init(&server, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        remote_poll_init(&nvs_handle, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        #endif
        
        #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
        browser_canvas_init(&server, display_text_buffer, DISPLAY_TEXTBUF_SIZE);
        telegram_bot_init(&nvs_handle, display_text_buffer, DISPLAY_TEXTBUF_SIZE);
        remote_poll_init(&nvs_handle, display_text_buffer, DISPLAY_TEXTBUF_SIZE);
        #endif
        
        #if defined(CONFIG_DISPLAY_TYPE_SELECTION)
        browser_canvas_init(&server, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        remote_poll_init(&nvs_handle, display_output_buffer, DISPLAY_FRAMEBUF_SIZE);
        #endif

        #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
        browser_canvas_register_brightness(&server, &display_brightness);
        #endif

        #if defined(CONFIG_DISPLAY_HAS_SHADERS)
        ESP_LOGI(LOG_TAG, "register shaders: %p", display_shader);
        browser_canvas_register_shaders(&server, &display_shader);
        #endif

        #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
        display_text_buffer[0] = '-';
        #endif

        xTaskCreatePinnedToCore(display_refresh_task, "display_refresh", 4096, NULL, 24, NULL, 1);

        #if defined(CONFIG_FAN_ENABLED)
        xTaskCreatePinnedToCore(fan_speed_adjust_task, "fan_speed_adjust", 4096, NULL, 3, NULL, 0);
        #endif
    }
}