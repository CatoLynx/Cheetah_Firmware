/*
 * Functions for LAWO ALUMA (XY10) flipdot panels
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "lawo_aluma.h"
#include "util.h"


#define LOG_TAG "LAWO-ALUMA"


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

void display_init() {
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

    display_set_address(7 - address);
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
    
    gpio_pulse(PIN_F, 1, FLIP_PULSE_WIDTH_US, FLIP_PULSE_WIDTH_US);
}

void display_set_backlight(uint8_t state) {
    /*
     * Set the backlight on or off
     */

    gpio_set_level(PIN_LED, state);
}

void display_render_frame_8bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    uint16_t p, x, y;
    if(prevFrame) {
        for(uint16_t i = 0; i < frameBufSize; i++) {
            if((frame[i] > 127) == (prevFrame[i] > 127)) continue;
            x = (i / CONFIG_DISPLAY_FRAME_HEIGHT);
            y = i % CONFIG_DISPLAY_FRAME_HEIGHT;
            p = x / 28; // TODO: Remove hardcoded 28
            x %= 28;
            if(i % (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT) == 0) display_select_panel(p);
            display_select_color(frame[i] > 127);
            display_select_column(x);
            display_select_row(y);
            ESP_LOGI(LOG_TAG, "frame[%d, %d, %d] = %d", p, x, y, frame[i]);
            display_flip();
        }
        memcpy(prevFrame, frame, frameBufSize);
    } else {
        for(uint16_t i = 0; i < frameBufSize; i++) {
            x = (i / CONFIG_DISPLAY_FRAME_HEIGHT);
            y = i % CONFIG_DISPLAY_FRAME_HEIGHT;
            p = x / 28; // TODO: Remove hardcoded 28
            x %= 28;
            if(i % (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT) == 0) display_select_panel(p);
            display_select_color(frame[i] > 127);
            display_select_column(x);
            display_select_row(y);
            ESP_LOGI(LOG_TAG, "frame[%d, %d, %d] = %d", p, x, y, frame[i]);
            display_flip();
        }
    }
    display_deselect();
}