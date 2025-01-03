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

#include "macros.h"

#include "config_global.h"
#include "artnet.h"
#if defined(DISPLAY_HAS_PIXEL_BUFFER)
#include "bitmap_generators.h"
#endif
#include "browser_canvas.h"
#include "browser_config.h"
#include "browser_spiffs.h"
#include "browser_ota.h"
#include "git_version.h"
#include "httpd.h"
#include "logging_tcp.h"
#include "playlist.h"
#include "telegram_bot.h"
#include "tpm2net.h"
#include "ethernet.h"
#include "wifi.h"
#include "util_brightness.h"
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
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_IBIS)
    #include "driver_display_char_ibis.h"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X_HYBRID)
    #include "driver_display_char_16seg_led_ws281x_hybrid.h"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_9000)
    #include "driver_display_sel_krone_9000.h"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_8200_PST)
    #include "driver_display_sel_krone_8200_pst.h"
#endif

#if defined(DISPLAY_HAS_TEXT_BUFFER)
    portMUX_TYPE display_text_buffer_lock = portMUX_INITIALIZER_UNLOCKED;
    uint8_t display_text_buffer[DISPLAY_TEXT_BUF_SIZE] = {0};
    uint8_t display_char_buffer[DISPLAY_CHAR_BUF_SIZE] = {0};
    uint16_t display_quirk_flags_buffer[DISPLAY_CHAR_BUF_SIZE] = {0};
    #if defined(CONFIG_DISPLAY_USE_PREV_TEXT_BUF)
    uint8_t display_prev_text_buffer[DISPLAY_TEXT_BUF_SIZE] = {0};
    #else
    uint8_t* display_prev_text_buffer = NULL;
    #endif
#endif

#if defined(CONFIG_DISPLAY_TYPE_SELECTION)
    // For selection displays, use an additional mask to determine which of the units
    // are present. This is necessary since configuration is done dynamically
    // using a JSON file in SPIFFS. This mask buffer conatins a 1 bit
    // if the unit at the given address is present, otherwise a 0 bit.
    uint8_t display_framebuf_mask[DIV_CEIL(DISPLAY_OUT_BUF_SIZE, 8)] = {0};
    uint16_t display_num_units = 0;
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    portMUX_TYPE display_pixel_buffer_lock = portMUX_INITIALIZER_UNLOCKED;
    uint8_t display_pixel_buffer[DISPLAY_PIX_BUF_SIZE] = {0};
    #if defined(CONFIG_DISPLAY_USE_PREV_PIX_BUF)
    uint8_t display_prev_pixel_buffer[DISPLAY_PIX_BUF_SIZE] = {0};
    #else
    uint8_t* display_prev_pixel_buffer = NULL;
    #endif

    #if defined(CONFIG_TPM2NET_FRAME_TYPE_1BPP)
        #define TPM2NET_FRAMEBUF_SIZE DISPLAY_PIX_BUF_SIZE_1BPP
    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_8BPP)
        #define TPM2NET_FRAMEBUF_SIZE DISPLAY_PIX_BUF_SIZE_8BPP
    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_24BPP)
        #define TPM2NET_FRAMEBUF_SIZE DISPLAY_PIX_BUF_SIZE_24BPP
    #endif
    uint8_t tpm2net_output_buffer[TPM2NET_FRAMEBUF_SIZE] = {0};

    #if defined(CONFIG_ARTNET_FRAME_TYPE_1BPP)
        #define ARTNET_FRAMEBUF_SIZE DISPLAY_PIX_BUF_SIZE_1BPP
    #elif defined(CONFIG_ARTNET_FRAME_TYPE_8BPP)
        #define ARTNET_FRAMEBUF_SIZE DISPLAY_PIX_BUF_SIZE_8BPP
    #elif defined(CONFIG_ARTNET_FRAME_TYPE_24BPP)
        #define ARTNET_FRAMEBUF_SIZE DISPLAY_PIX_BUF_SIZE_24BPP
    #endif
    uint8_t artnet_output_buffer[ARTNET_FRAMEBUF_SIZE] = {0};
#endif

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
    uint8_t display_prevBrightness = 0;
    uint8_t display_brightness = 255;
    #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_SENSOR)
    uint8_t display_baseBrightness = 127;
    #endif
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
    cJSON* display_prevShader = NULL;
    cJSON* display_shader = NULL;
    uint8_t display_shaderDataDeletable = 0;
    uint8_t display_prevShaderDataDeletable = 0;
#endif

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
    cJSON* display_prevTransition = NULL;
    cJSON* display_transition = NULL;
    uint8_t display_transitionDataDeletable = 0;
    uint8_t display_prevTransitionDataDeletable = 0;
#endif

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
    cJSON* display_prevEffect = NULL;
    cJSON* display_effect = NULL;
    uint8_t display_effectDataDeletable = 0;
    uint8_t display_prevEffectDataDeletable = 0;
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    cJSON* display_prevBitmapGenerator = NULL;
    cJSON* display_bitmapGenerator = NULL;
    uint8_t display_bitmapGeneratorDataDeletable = 0;
    uint8_t display_prevBitmapGeneratorDataDeletable = 0;
#endif

size_t hostname_length = 64;
char hostname[64];


static void display_refresh_task(void* arg) {
    // Start with splashscreen
    #if defined(DISPLAY_HAS_TEXT_BUFFER)
        char hostname_upper[64];
        strncpy(hostname_upper, hostname, 63);
        str_toUpper(hostname_upper);

        char splash_text[74];

        #if defined(GIT_VERSION)
            snprintf(splash_text, 74, "%s %s", hostname_upper, GIT_VERSION);
        #else
            snprintf(splash_text, 74, "%s", hostname_upper);
        #endif

        #if defined(CONFIG_DISPLAY_SHOW_MESSAGES)
            STRCPY_TEXTBUF((char*)display_text_buffer, splash_text, DISPLAY_TEXT_BUF_SIZE);

            #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
                display_update(display_text_buffer, display_prev_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, display_char_buffer, display_quirk_flags_buffer, DISPLAY_CHAR_BUF_SIZE);
                #if defined(CONFIG_DISPLAY_USE_PREV_TEXT_BUF)
                memcpy(display_prev_text_buffer, display_text_buffer, DISPLAY_TEXT_BUF_SIZE);
                #endif
            #endif

            taskYIELD();
            vTaskDelay(5000 / portTICK_PERIOD_MS);
        #endif
    #endif

    while (1) {
        #if defined(DISPLAY_HAS_PIXEL_BUFFER)
        bitmap_generator_current(time_getSystemTime_us());
        #endif

        #if defined(CONFIG_DISPLAY_TYPE_PIXEL)
            display_update(display_pixel_buffer, display_prev_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock);
            #if defined(CONFIG_DISPLAY_USE_PREV_PIX_BUF)
            // TODO: OBSOLETE AGAIN - DRIVER MUST HANDLE
            //memcpy(display_prev_pixel_buffer, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE);
            #endif
        #elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
            display_update(display_text_buffer, display_prev_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, display_char_buffer, display_quirk_flags_buffer, DISPLAY_CHAR_BUF_SIZE);
            #if defined(CONFIG_DISPLAY_USE_PREV_TEXT_BUF)
            // TODO: OBSOLETE AGAIN - DRIVER MUST HANDLE
            //memcpy(display_prev_text_buffer, display_text_buffer, DISPLAY_TEXT_BUF_SIZE);
            #endif
        #elif defined(CONFIG_DISPLAY_TYPE_CHAR_ON_PIXEL) || defined(CONFIG_DISPLAY_TYPE_PIXEL_ON_CHAR)
            display_update(display_pixel_buffer, display_prev_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, display_text_buffer, display_prev_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, display_char_buffer, display_quirk_flags_buffer, DISPLAY_CHAR_BUF_SIZE);
            #if defined(CONFIG_DISPLAY_USE_PREV_PIX_BUF)
            // TODO: OBSOLETE AGAIN - DRIVER MUST HANDLE
            //memcpy(display_prev_pixel_buffer, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock);
            #endif
            #if defined(CONFIG_DISPLAY_USE_PREV_TEXT_BUF)
            // TODO: OBSOLETE AGAIN - DRIVER MUST HANDLE
            //memcpy(display_prev_text_buffer, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock);
            #endif
        #elif defined(CONFIG_DISPLAY_TYPE_SELECTION)
            // TODO: Use a separate unit buffer instead of writing directly to the output buffer
            // and actually update the display. Selection displays currently don't update at all
            // ever since the buffer structure has changed.
            // The unit buffer currently does not exist yet.
        #endif

        #if defined(CONFIG_FAN_ENABLED)
        fan_set_target_speed(display_get_fan_speed());
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
            if (display_prevShader != NULL && display_prevShaderDataDeletable) {
                cJSON_Delete(display_prevShader);
            }
            display_prevShader = display_shader;
            display_prevShaderDataDeletable = display_shaderDataDeletable;
        }
        #endif

        #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
        if (display_transition != display_prevTransition) {
            display_set_transition(display_transition);
            
            // Delete previous transition data if necessary
            if (display_prevTransition != NULL && display_prevTransitionDataDeletable) {
                cJSON_Delete(display_prevTransition);
            }
            display_prevTransition = display_transition;
            display_prevTransitionDataDeletable = display_transitionDataDeletable;
        }
        #endif

        #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
        if (display_effect != display_prevEffect) {
            display_set_effect(display_effect);
            
            // Delete previous effect data if necessary
            if (display_prevEffect != NULL && display_prevEffectDataDeletable) {
                cJSON_Delete(display_prevEffect);
            }
            display_prevEffect = display_effect;
            display_prevEffectDataDeletable = display_effectDataDeletable;
        }
        #endif

        #if defined(DISPLAY_HAS_PIXEL_BUFFER)
        if (display_bitmapGenerator != display_prevBitmapGenerator) {
            bitmap_generator_select(display_bitmapGenerator);
            
            // Delete previous bitmap generator data if necessary
            if (display_prevBitmapGenerator != NULL && display_prevBitmapGeneratorDataDeletable) {
                cJSON_Delete(display_prevBitmapGenerator);
            }
            display_prevBitmapGenerator = display_bitmapGenerator;
            display_prevBitmapGeneratorDataDeletable = display_bitmapGeneratorDataDeletable;
        }
        #endif

        taskYIELD();
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

    #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL) && defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_SENSOR)
    ESP_ERROR_CHECK(brightness_sensor_init(&display_brightness, &display_baseBrightness));
    #endif

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wg_init(&nvs_handle); // Init WireGuard before WiFi and Ethernet to ensure it's ready when WiFi / Ethernet gets an IP
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

    #if defined(DISPLAY_HAS_PIXEL_BUFFER) && defined(CONFIG_DISPLAY_PIX_BUF_INIT_WHITE)
    memset(display_pixel_buffer, 0xFF, DISPLAY_PIX_BUF_SIZE);
    #endif

    #if defined(CONFIG_DISPLAY_TYPE_SELECTION)
    ret = display_init(&nvs_handle, display_framebuf_mask, &display_num_units);
    #else
    ret = display_init(&nvs_handle);
    #endif
    if (ret == ESP_OK) {
        #if defined(CONFIG_DISPLAY_TYPE_PIXEL)
        tpm2net_init(display_pixel_buffer, tpm2net_output_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, TPM2NET_FRAMEBUF_SIZE);
        artnet_init(display_pixel_buffer, artnet_output_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, ARTNET_FRAMEBUF_SIZE);
        browser_canvas_init(&server, &nvs_handle, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, NULL, 0, NULL, NULL, 0, NULL);
        playlist_init(&nvs_handle, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, NULL, 0, NULL, NULL, 0, NULL);
        bitmap_generators_init(display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, DISPLAY_VIEWPORT_WIDTH_PIXEL, DISPLAY_VIEWPORT_HEIGHT_PIXEL);
        #endif
        
        #if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
        browser_canvas_init(&server, &nvs_handle, NULL, 0, NULL, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, NULL, 0, NULL);
        telegram_bot_init(&nvs_handle, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock);
        playlist_init(&nvs_handle, NULL, 0, NULL, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, NULL, 0, NULL);
        #endif

        #if defined(CONFIG_DISPLAY_TYPE_CHAR_ON_PIXEL) || defined(CONFIG_DISPLAY_TYPE_PIXEL_ON_CHAR)
        tpm2net_init(display_pixel_buffer, tpm2net_output_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, TPM2NET_FRAMEBUF_SIZE);
        artnet_init(display_pixel_buffer, artnet_output_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, ARTNET_FRAMEBUF_SIZE);
        browser_canvas_init(&server, &nvs_handle, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, NULL, 0, NULL);
        playlist_init(&nvs_handle, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, &display_text_buffer_lock, NULL, 0, NULL);
        bitmap_generators_init(display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, &display_pixel_buffer_lock, DISPLAY_VIEWPORT_WIDTH_PIXEL, DISPLAY_VIEWPORT_HEIGHT_PIXEL);
        #endif
        
        #if defined(CONFIG_DISPLAY_TYPE_SELECTION)
        browser_canvas_init(&server, &nvs_handle, NULL, 0, NULL, NULL, 0, NULL, display_unit_buffer, DISPLAY_UNIT_BUF_SIZE, &display_unit_buffer_lock);
        playlist_init(&nvs_handle, NULL, 0, NULL, NULL, 0, NULL, display_unit_buffer, DISPLAY_UNIT_BUF_SIZE, &display_unit_buffer_lock);
        #endif

        #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
            #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_SENSOR)
            browser_canvas_register_brightness(&server, &display_baseBrightness);
            playlist_register_brightness(&display_baseBrightness);
            #else
            browser_canvas_register_brightness(&server, &display_brightness);
            playlist_register_brightness(&display_brightness);
            #endif
            #if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_SENSOR)
            xTaskCreatePinnedToCore(brightness_adjust_task, "brightness_adjust", 4096, NULL, 3, NULL, 0);
            #endif
        #endif

        #if defined(CONFIG_DISPLAY_HAS_SHADERS)
        ESP_LOGI(LOG_TAG, "Registering shaders");
        browser_canvas_register_shaders(&server, &display_shader, &display_shaderDataDeletable);
        playlist_register_shaders(&display_shader, &display_shaderDataDeletable);
        #endif

        #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
        ESP_LOGI(LOG_TAG, "Registering transitions");
        browser_canvas_register_transitions(&server, &display_transition, &display_transitionDataDeletable);
        playlist_register_transitions(&display_transition, &display_transitionDataDeletable);
        #endif

        #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
        ESP_LOGI(LOG_TAG, "Registering effects");
        browser_canvas_register_effects(&server, &display_effect, &display_effectDataDeletable);
        playlist_register_effects(&display_effect, &display_effectDataDeletable);
        #endif

        #if defined(DISPLAY_HAS_PIXEL_BUFFER)
        ESP_LOGI(LOG_TAG, "Registering bitmap generators");
        browser_canvas_register_bitmap_generators(&server, &display_bitmapGenerator, &display_bitmapGeneratorDataDeletable);
        playlist_register_bitmap_generators(&display_bitmapGenerator, &display_bitmapGeneratorDataDeletable);
        #endif

        // Load startup defaults
        cJSON* startupData;
        char* startupFile = get_string_from_nvs(&nvs_handle, "startup_file");
        if (startupFile == NULL) {
            ESP_LOGW(LOG_TAG, "Not using startup file");
        } else {
            esp_err_t ret = get_json_from_spiffs(startupFile, &startupData, LOG_TAG);

            if (ret != ESP_OK || !cJSON_IsObject(startupData)) {
                ESP_LOGE(LOG_TAG, "Failed to load startup file");
            } else {
                cJSON* buffers_field = cJSON_GetObjectItem(startupData, "buffers");
                if (cJSON_IsObject(buffers_field)) {
                    #if defined(DISPLAY_HAS_PIXEL_BUFFER)
                    cJSON* pixbuf_field = cJSON_GetObjectItem(buffers_field, "pixel");
                    uint8_t pixbuf_b64 = 0;
                    if (pixbuf_field == NULL || cJSON_IsNull(pixbuf_field)) {
                        pixbuf_field = cJSON_GetObjectItem(buffers_field, "pixel_b64");
                        pixbuf_b64 = 1;
                    }
                    if (pixbuf_field != NULL && !cJSON_IsNull(pixbuf_field)) {
                        char* buffer_str = cJSON_GetStringValue(pixbuf_field);
                        buffer_from_string(buffer_str, pixbuf_b64, display_pixel_buffer, DISPLAY_PIX_BUF_SIZE, LOG_TAG);
                    }
                    #endif
                    
                    #if defined(DISPLAY_HAS_TEXT_BUFFER)
                    cJSON* textbuf_field = cJSON_GetObjectItem(buffers_field, "text");
                    uint8_t textbuf_b64 = 0;
                    if (textbuf_field == NULL || cJSON_IsNull(textbuf_field)) {
                        textbuf_field = cJSON_GetObjectItem(buffers_field, "text_b64");
                        textbuf_b64 = 1;
                    }
                    if (textbuf_field != NULL && !cJSON_IsNull(textbuf_field)) {
                        char* buffer_str = cJSON_GetStringValue(textbuf_field);
                        buffer_from_string(buffer_str, textbuf_b64, display_text_buffer, DISPLAY_TEXT_BUF_SIZE, LOG_TAG);
                    }
                    #endif
                    
                    #if defined(CONFIG_DISPLAY_TYPE_SELECTION)
                    cJSON* unitbuf_field = cJSON_GetObjectItem(buffers_field, "unit");
                    uint8_t unitbuf_b64 = 0;
                    if (unitbuf_field == NULL || cJSON_IsNull(unitbuf_field)) {
                        unitbuf_field = cJSON_GetObjectItem(buffers_field, "unit_b64");
                        unitbuf_b64 = 1;
                    }
                    if (unitbuf_field != NULL && !cJSON_IsNull(unitbuf_field)) {
                        char* buffer_str = cJSON_GetStringValue(unitbuf_field);
                        buffer_from_string(buffer_str, unitbuf_b64, display_unit_buffer, DISPLAY_UNIT_BUF_SIZE, LOG_TAG);
                    }
                    #endif
                }

                #if defined(CONFIG_DISPLAY_HAS_SHADERS)
                cJSON* shader_field = cJSON_GetObjectItem(startupData, "shader");
                if (cJSON_IsObject(shader_field)) {
                    display_shader = cJSON_Duplicate(shader_field, true);
                    display_shaderDataDeletable = 1;
                }
                #endif

                #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
                cJSON* transition_field = cJSON_GetObjectItem(startupData, "transition");
                if (cJSON_IsObject(transition_field)) {
                    display_transition = cJSON_Duplicate(transition_field, true);
                    display_transitionDataDeletable = 1;
                }
                #endif

                #if defined(CONFIG_DISPLAY_HAS_EFFECTS)
                cJSON* effect_field = cJSON_GetObjectItem(startupData, "effect");
                if (cJSON_IsObject(effect_field)) {
                    display_effect = cJSON_Duplicate(effect_field, true);
                    display_effectDataDeletable = 1;
                }
                #endif

                #if defined(DISPLAY_HAS_PIXEL_BUFFER)
                cJSON* bitmap_generator_field = cJSON_GetObjectItem(startupData, "bitmap_generator");
                if (cJSON_IsObject(bitmap_generator_field)) {
                    display_bitmapGenerator = cJSON_Duplicate(bitmap_generator_field, true);
                    display_bitmapGeneratorDataDeletable = 1;
                }
                #endif

                cJSON_Delete(startupData);
            }
        }
        free(startupFile);
        
        xTaskCreatePinnedToCore(wifi_timeout_task, "wifi_timeout", 4096, NULL, 2, NULL, 0);
        xTaskCreatePinnedToCore(display_refresh_task, "display_refresh", 4096, NULL, 24, NULL, 1);
        #if defined(CONFIG_FAN_ENABLED)
        xTaskCreatePinnedToCore(fan_speed_adjust_task, "fan_speed_adjust", 4096, NULL, 3, NULL, 0);
        #endif
    }
}