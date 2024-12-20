/*
 * Functions for LAWO ALUMA (XY10) flipdot panels
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "lawo_aluma.h"
#include "util_gpio.h"

#if defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_LAWO_ALUMA)


#define LOG_TAG "LAWO-ALUMA"

static uint8_t display_outBuf[OUTPUT_BUFFER_SIZE] = {0};
static uint8_t display_dirty = 1;


static const uint8_t rowLookupTableYellow[28] = {
    2, 3, 6, 7, 1, 0, 5, 4,
    10, 11, 14, 15, 9, 8, 13, 12,
    18, 19, 22, 23, 17, 16, 21, 20,
    30, 31, 29, 28
};

static const uint8_t rowLookupTableBlack[28] = {
    7, 5, 3, 1, 4, 6, 0, 2,
    15, 13, 11, 9, 12, 14, 8, 10,
    23, 21, 19, 17, 20, 22, 16, 18,
    27, 25, 24, 26
};

static uint8_t currentColor = 0;

esp_err_t display_init(nvs_handle_t* nvsHandle) {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(PIN_CS);
    gpio_reset_pin(PIN_EL);
    gpio_reset_pin(PIN_ER);
    gpio_reset_pin(PIN_LC_N);
    gpio_reset_pin(PIN_LP_N);
    gpio_reset_pin(PIN_F);
    gpio_reset_pin(PIN_A0);
    gpio_reset_pin(PIN_A1);
    gpio_reset_pin(PIN_A2);
    gpio_reset_pin(PIN_A3);
    gpio_reset_pin(PIN_COL_A3);
    gpio_reset_pin(PIN_LED);

    gpio_set_direction(PIN_CS, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_EL, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_ER, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LC_N, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LP_N, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_F, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_A0, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_A1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_A2, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_A3, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_COL_A3, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_LED, GPIO_MODE_OUTPUT);
    
    gpio_set_level(PIN_CS, 0);
    gpio_set_level(PIN_EL, 0);
    gpio_set_level(PIN_ER, 0);
    gpio_set_level(PIN_LC_N, 1);
    gpio_set_level(PIN_LP_N, 1);
    gpio_set_level(PIN_F, 0);
    gpio_set_level(PIN_A0, 0);
    gpio_set_level(PIN_A1, 0);
    gpio_set_level(PIN_A2, 0);
    gpio_set_level(PIN_A3, 0);
    gpio_set_level(PIN_COL_A3, 0);
    gpio_set_level(PIN_LED, 0);
    return ESP_OK;
}

void display_set_address(uint8_t address) {
    /*
     * Output a 4-bit address on the address bus
     */

    gpio_set_level(PIN_A0, ((address & 1) != 0));
    gpio_set_level(PIN_A1, ((address & 2) != 0));
    gpio_set_level(PIN_A2, ((address & 4) != 0));
    gpio_set_level(PIN_A3, ((address & 8) != 0));
}

void display_select_row(uint8_t address) {
    /*
     * Select a row
     */

    display_set_address(address);
}

void display_select_column(uint8_t address) {
    /*
     * Select a column and latch the address
     */

    uint8_t colAddress;

    if(currentColor == 1) {
        colAddress = rowLookupTableYellow[address];
    } else {
        colAddress = rowLookupTableBlack[address];
    }

    display_set_address(colAddress);
    gpio_pulse(PIN_LC_N, 0, LATCH_PULSE_WIDTH_US, LATCH_PULSE_WIDTH_US);

    gpio_set_level(PIN_COL_A3, ((colAddress & 8) != 0));

    if(colAddress >= 16) {
        gpio_set_level(PIN_EL, 0);
        gpio_set_level(PIN_ER, 1);
    } else {
        gpio_set_level(PIN_ER, 0);
        gpio_set_level(PIN_EL, 1);
    }
}

void display_select_panel(uint8_t address) {
    /*
     * Select a panel and latch the address
     */

    display_set_address(CONFIG_ALUMA_NUM_PANELS - (7 - address) - 1);
    gpio_pulse(PIN_LP_N, 0, LATCH_PULSE_WIDTH_US, LATCH_PULSE_WIDTH_US);
}

void display_select_color(uint8_t color) {
    /*
     * Select the color to flip to.
     * 1 = yellow
     * 0 = black
     */
    
    gpio_set_level(PIN_CS, color);
    currentColor = color;
}

void display_deselect() {
    /*
     * Deselect all control lines
     */

    gpio_set_level(PIN_EL, 0);
    gpio_set_level(PIN_ER, 0);

    gpio_set_level(PIN_COL_A3, 0);
}

void display_flip() {
    /*
     * Flip the currently selected pixel
     */
    
    gpio_pulse(PIN_F, 1, FLIP_PULSE_WIDTH_US, FLIP_PAUSE_US);
}

void display_set_backlight(uint8_t state) {
    /*
     * Set the backlight on or off
     */

    gpio_set_level(PIN_LED, state);
}

void display_render() {
    uint16_t prev_p, p, x, y;
    prev_p = 65535;
    for(size_t i = 0; i < OUTPUT_BUFFER_SIZE; i++) {
        if (display_outBuf[i] == 0xFF) continue;
        x = (i / CONFIG_DISPLAY_FRAME_HEIGHT_PIXEL);
        y = i % CONFIG_DISPLAY_FRAME_HEIGHT_PIXEL;
        p = x / CONFIG_ALUMA_PANEL_WIDTH;
        x %= CONFIG_ALUMA_PANEL_WIDTH;
        if (p != prev_p) {
            ESP_LOGV(LOG_TAG, "Selecting panel %d", p);
            display_select_panel(p);
            prev_p = p;
        }
        display_select_color(display_outBuf[i]);
        display_select_column(x);
        display_select_row(y);
        ESP_LOGV(LOG_TAG, "display_outBuf[%d, %d, %d] = %d", p, x, y, display_outBuf[i]);
        display_flip();
    }
    display_dirty = 0;
    display_deselect();
}

void display_buffers_to_out_buf(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize) {
    // 0 = Set pixel black
    // 1 = Set pixel yellow
    // 0xFF = Skip pixel
    memset(display_outBuf, 0xFF, OUTPUT_BUFFER_SIZE);

    if (prevPixBuf != NULL && !display_dirty) {
        for (size_t i = 0; i < pixBufSize; i++) {
            if (prevPixBuf[i] > 127 && pixBuf[i] <= 127) {
                display_outBuf[i] = 0;
            } else if (prevPixBuf[i] <= 127 && pixBuf[i] > 127) {
                display_outBuf[i] = 1;
            }
        }
    } else {
        for (size_t i = 0; i < pixBufSize; i++) {
            if (pixBuf[i] <= 127) {
                display_outBuf[i] = 0;
            } else {
                display_outBuf[i] = 1;
            }
        }
    }
}

void display_update(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock) {
    // Nothing to do if buffer hasn't changed
    if (prevPixBuf != NULL && memcmp(pixBuf, prevPixBuf, pixBufSize) == 0) return;

    taskENTER_CRITICAL(pixBufLock);
    display_buffers_to_out_buf(pixBuf, prevPixBuf, pixBufSize);
    if (prevPixBuf != NULL) memcpy(prevPixBuf, pixBuf, pixBufSize);
    taskEXIT_CRITICAL(pixBufLock);
    display_render();
}

#endif