/*
 * Functions for WS281x based LED displays
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "macros.h"
#include "char_16seg_led_ws281x.h"
#include "util_generic.h"
#include "char_16seg_font.h"
#include "driver/rmt.h"
#include "led_strip.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X)


#define LOG_TAG "CH-16SEG-LED-WS281X"

uint8_t display_currentBrightness = 255;
color_t OFF = { 0, 0, 0 };
led_strip_t* strip;


void display_init() {
    /*
     * Set up all needed peripherals
     */

    rmt_config_t config = RMT_DEFAULT_CONFIG_TX(CONFIG_16SEG_WS281X_DATA_IO, CONFIG_16SEG_WS281X_RMT_CH);
    config.clk_div = 2;
    ESP_ERROR_CHECK(rmt_config(&config));
    ESP_ERROR_CHECK(rmt_driver_install(config.channel, 0, ESP_INTR_FLAG_IRAM));

    led_strip_config_t strip_config = LED_STRIP_DEFAULT_CONFIG(WS281X_NUM_LEDS, (led_strip_dev_t)config.channel);
    strip = led_strip_new_rmt_ws2812(&strip_config);
    if (!strip) {
        ESP_LOGE(LOG_TAG, "Failed to install WS281x driver");
        return;
    }
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
esp_err_t display_set_brightness(uint8_t brightness) {
    display_currentBrightness = brightness;
    return ESP_OK;
}
#else
esp_err_t display_set_brightness(uint8_t brightness) {
    return ESP_OK;
}
#endif

uint32_t display_calculateFrameBufferCharacterIndex(uint16_t charPos) {
    // Calculate the frame buffer start index for the given character position
    // Position starts at 0 in the top left corner
    uint32_t fb_index = 0;
    fb_index = DIV_CEIL(CONFIG_DISPLAY_FRAMEBUF_BITS_PER_CHAR, 8) * (DISPLAY_CHARBUF_SIZE - charPos - 1);
    return fb_index;
}

void display_setLEDColor(uint8_t* frameBuf, uint16_t ledPos, color_t color) {
    frameBuf[ledPos * 3 + 0] = color.red;
    frameBuf[ledPos * 3 + 1] = color.green;
    frameBuf[ledPos * 3 + 2] = color.blue;
}

void display_setDecimalPointAt(uint8_t* frameBuf, uint16_t charPos, uint8_t state, color_t color) {
    uint32_t fb_base_i = display_calculateFrameBufferCharacterIndex(charPos);

    if (state) {
        display_setLEDColor(&frameBuf[fb_base_i], 48, color);
    } else {
        display_setLEDColor(&frameBuf[fb_base_i], 48, OFF);
    }
}

void display_setCharDataAt(uint8_t* frameBuf, uint16_t charPos, uint16_t charData, color_t color) {
    uint32_t fb_base_i = display_calculateFrameBufferCharacterIndex(charPos);
    memset(&frameBuf[fb_base_i], 0x00, 3 * 48);
    
    if (charData & A) {
        display_setLEDColor(&frameBuf[fb_base_i], 0, color);
        display_setLEDColor(&frameBuf[fb_base_i], 1, color);
    }
    if (charData & B) {
        display_setLEDColor(&frameBuf[fb_base_i], 2, color);
        display_setLEDColor(&frameBuf[fb_base_i], 3, color);
    }
    if (charData & C) {
        display_setLEDColor(&frameBuf[fb_base_i], 4, color);
        display_setLEDColor(&frameBuf[fb_base_i], 5, color);
        display_setLEDColor(&frameBuf[fb_base_i], 6, color);
        display_setLEDColor(&frameBuf[fb_base_i], 7, color);
    }
    if (charData & D) {
        display_setLEDColor(&frameBuf[fb_base_i], 8, color);
        display_setLEDColor(&frameBuf[fb_base_i], 9, color);
        display_setLEDColor(&frameBuf[fb_base_i], 10, color);
        display_setLEDColor(&frameBuf[fb_base_i], 11, color);
    }
    if (charData & E) {
        display_setLEDColor(&frameBuf[fb_base_i], 12, color);
        display_setLEDColor(&frameBuf[fb_base_i], 13, color);
    }
    if (charData & F) {
        display_setLEDColor(&frameBuf[fb_base_i], 14, color);
        display_setLEDColor(&frameBuf[fb_base_i], 15, color);
    }
    if (charData & G) {
        display_setLEDColor(&frameBuf[fb_base_i], 16, color);
        display_setLEDColor(&frameBuf[fb_base_i], 17, color);
        display_setLEDColor(&frameBuf[fb_base_i], 18, color);
        display_setLEDColor(&frameBuf[fb_base_i], 19, color);
    }
    if (charData & H) {
        display_setLEDColor(&frameBuf[fb_base_i], 20, color);
        display_setLEDColor(&frameBuf[fb_base_i], 21, color);
        display_setLEDColor(&frameBuf[fb_base_i], 22, color);
        display_setLEDColor(&frameBuf[fb_base_i], 23, color);
    }
    if (charData & K) {
        display_setLEDColor(&frameBuf[fb_base_i], 24, color);
        display_setLEDColor(&frameBuf[fb_base_i], 25, color);
        display_setLEDColor(&frameBuf[fb_base_i], 26, color);
    }
    if (charData & M) {
        display_setLEDColor(&frameBuf[fb_base_i], 27, color);
        display_setLEDColor(&frameBuf[fb_base_i], 28, color);
        display_setLEDColor(&frameBuf[fb_base_i], 29, color);
        display_setLEDColor(&frameBuf[fb_base_i], 30, color);
    }
    if (charData & N) {
        display_setLEDColor(&frameBuf[fb_base_i], 31, color);
        display_setLEDColor(&frameBuf[fb_base_i], 32, color);
        display_setLEDColor(&frameBuf[fb_base_i], 33, color);
    }
    if (charData & R) {
        display_setLEDColor(&frameBuf[fb_base_i], 45, color);
        display_setLEDColor(&frameBuf[fb_base_i], 46, color);
        display_setLEDColor(&frameBuf[fb_base_i], 47, color);
    }
    if (charData & S) {
        display_setLEDColor(&frameBuf[fb_base_i], 41, color);
        display_setLEDColor(&frameBuf[fb_base_i], 42, color);
        display_setLEDColor(&frameBuf[fb_base_i], 43, color);
        display_setLEDColor(&frameBuf[fb_base_i], 44, color);
    }
    if (charData & T) {
        display_setLEDColor(&frameBuf[fb_base_i], 38, color);
        display_setLEDColor(&frameBuf[fb_base_i], 39, color);
        display_setLEDColor(&frameBuf[fb_base_i], 40, color);
    }
    if (charData & U) {
        display_setLEDColor(&frameBuf[fb_base_i], 36, color);
        display_setLEDColor(&frameBuf[fb_base_i], 37, color);
    }
    if (charData & P) {
        display_setLEDColor(&frameBuf[fb_base_i], 34, color);
        display_setLEDColor(&frameBuf[fb_base_i], 35, color);
    }
}

void display_charbuf_to_framebuf(uint8_t* charBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize) {
    uint8_t prevWasLetter = 0;
    uint16_t decPointMergeCnt = 0;
    uint16_t cb_i_display = 0;
    color_t color = {255, 0, 128};

    memset(frameBuf, 0x00, frameBufSize);

    for (uint16_t cb_i_source = 0; cb_i_source < charBufSize; cb_i_source++) {
        if (charBuf[cb_i_source] == '.') {
            if (!prevWasLetter) {
                // Current char is . and previous was . too
                display_setDecimalPointAt(frameBuf, cb_i_display, 1, color);
            } else {
                // Current char is . but previous was a letter
                // Add the point to the previous letter and update the respective counter
                cb_i_display--;
                display_setDecimalPointAt(frameBuf, cb_i_display, 1, color);
                decPointMergeCnt++;
                prevWasLetter = 0;
            }
        } else {
            // Current char is a letter
            if (charBuf[cb_i_source] >= char_seg_font_min && charBuf[cb_i_source] <= char_seg_font_max) {
                display_setCharDataAt(frameBuf, cb_i_display, char_16seg_font[charBuf[cb_i_source] - char_seg_font_min], color);
            }
            prevWasLetter = 1;
        }

        cb_i_display++;
    }
}

void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    ESP_LOGV(LOG_TAG, "Rendering frame");
    for (uint16_t i = 0; i < WS281X_NUM_LEDS; i++) {
        strip->set_pixel(strip, i, ((uint16_t)frame[i * 3 + 0] * display_currentBrightness) / 255, ((uint16_t)frame[i * 3 + 1] * display_currentBrightness) / 255, ((uint16_t)frame[i * 3 + 2] * display_currentBrightness) / 255);
    }
    strip->refresh(strip, 100);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

#endif