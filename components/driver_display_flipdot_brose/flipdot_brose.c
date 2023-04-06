/*
 * Functions for BROSE flipdot panels
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "flipdot_brose.h"
#include "util_gpio.h"
#include "macros.h"

#if defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_BROSE)


#define LOG_TAG "FLIPDOT-BROSE"

static uint8_t display_dirty = 1;

esp_err_t display_init(nvs_handle_t* nvsHandle) {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_BROSE_RD_EN_R_IO);
    gpio_reset_pin(CONFIG_BROSE_RD_EN_S_IO);
    gpio_reset_pin(CONFIG_BROSE_RD_A0_IO);
    gpio_reset_pin(CONFIG_BROSE_RD_A1_IO);
    gpio_reset_pin(CONFIG_BROSE_RD_A2_IO);
    gpio_reset_pin(CONFIG_BROSE_PAN_E_IO);
    gpio_reset_pin(CONFIG_BROSE_PAN_CS_IO);
    gpio_reset_pin(CONFIG_BROSE_COL_A0_IO);
    gpio_reset_pin(CONFIG_BROSE_COL_A1_IO);
    gpio_reset_pin(CONFIG_BROSE_COL_A2_IO);
    gpio_reset_pin(CONFIG_BROSE_COL_B0_IO);
    gpio_reset_pin(CONFIG_BROSE_COL_B1_IO);

    gpio_set_direction(CONFIG_BROSE_RD_EN_R_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_RD_EN_S_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_RD_A0_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_RD_A1_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_RD_A2_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_PAN_E_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_PAN_CS_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_COL_A0_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_COL_A1_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_COL_A2_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_COL_B0_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_BROSE_COL_B1_IO, GPIO_MODE_OUTPUT);
    
    display_deselect();
    return ESP_OK;
}

void display_select_column(uint8_t address) {
    /*
     * Output a 5-bit address on the column address bus
     */

    // Leftmost column is the highest address, so invert
    address = CONFIG_BROSE_PANEL_WIDTH - address - 1;

    // A quirk of the FP2800 chip used to drive the columns is that ((addresses divisible by 8) - 1) are not used, so we need to skip those
    // This is contrary to what the datasheet states
    address += address / 7;

    gpio_set_level(CONFIG_BROSE_COL_A0_IO, ((address & 1) != 0));
    gpio_set_level(CONFIG_BROSE_COL_A1_IO, ((address & 2) != 0));
    gpio_set_level(CONFIG_BROSE_COL_A2_IO, ((address & 4) != 0));
    gpio_set_level(CONFIG_BROSE_COL_B0_IO, ((address & 8) != 0));
    gpio_set_level(CONFIG_BROSE_COL_B1_IO, ((address & 16) != 0));
}

void display_select_row(uint8_t address) {
    /*
     * Output a 3-bit address on the row address bus
     */

    gpio_set_level(CONFIG_BROSE_RD_A0_IO, ((address & 1) != 0));
    gpio_set_level(CONFIG_BROSE_RD_A1_IO, ((address & 2) != 0));
    gpio_set_level(CONFIG_BROSE_RD_A2_IO, ((address & 4) != 0));
}

void display_select_color(uint8_t color) {
    /*
     * Select the color to flip to.
     * 1 = yellow
     * 0 = black
     */

    if (color) {
        gpio_set_level(CONFIG_BROSE_RD_EN_R_IO, 0);
        ets_delay_us(1);
        gpio_set_level(CONFIG_BROSE_RD_EN_S_IO, 1);
    } else {
        gpio_set_level(CONFIG_BROSE_RD_EN_S_IO, 0);
        ets_delay_us(1);
        gpio_set_level(CONFIG_BROSE_RD_EN_R_IO, 1);
    }
    gpio_set_level(CONFIG_BROSE_PAN_CS_IO, color);
}

void display_deselect() {
    /*
     * Deselect all control lines
     */

    gpio_set_level(CONFIG_BROSE_RD_EN_R_IO, 0);
    gpio_set_level(CONFIG_BROSE_RD_EN_S_IO, 0);
    gpio_set_level(CONFIG_BROSE_RD_A0_IO, 0);
    gpio_set_level(CONFIG_BROSE_RD_A1_IO, 0);
    gpio_set_level(CONFIG_BROSE_RD_A2_IO, 0);
    gpio_set_level(CONFIG_BROSE_PAN_E_IO, 0);
    gpio_set_level(CONFIG_BROSE_PAN_CS_IO, 1);
    gpio_set_level(CONFIG_BROSE_COL_A0_IO, 1);
    gpio_set_level(CONFIG_BROSE_COL_A1_IO, 1);
    gpio_set_level(CONFIG_BROSE_COL_A2_IO, 1);
    gpio_set_level(CONFIG_BROSE_COL_B0_IO, 1);
    gpio_set_level(CONFIG_BROSE_COL_B1_IO, 1);
}

void display_flip() {
    /*
     * Flip the currently selected pixel
     */
    
    gpio_pulse(CONFIG_BROSE_PAN_E_IO, 1, CONFIG_BROSE_FLIP_PULSE_WIDTH, CONFIG_BROSE_FLIP_PAUSE_LENGTH);
}

void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    uint16_t prev_p, p, x, y_byte, y;
    prev_p = 0xFFFF;
    if(!display_dirty && prevFrame) {
        for(uint16_t i = 0; i < frameBufSize; i++) {
            x = (i / DISPLAY_FRAME_HEIGHT_BYTES);
            y_byte = i % DISPLAY_FRAME_HEIGHT_BYTES;
            for (uint8_t y_bit = 0; y_bit < 7; y_bit++) {
                y = (y_byte * 8) + y_bit;
                if(BUFFER_VAL(frame, x, y) == BUFFER_VAL(prevFrame, x, y)) continue;
                p = x / CONFIG_BROSE_PANEL_WIDTH;
                if (p != prev_p) {
                    //ESP_LOGV(LOG_TAG, "Selecting panel %d", p);
                    //display_select_panel(p);
                    prev_p = p;
                }
                display_select_color(BUFFER_VAL(frame, x, y));
                display_select_column(x % CONFIG_BROSE_PANEL_WIDTH);
                display_select_row(y);
                ESP_LOGV(LOG_TAG, "frame[%d, %d, %d] = %d", p, x, y, frame[i]);
                display_flip();
            }
        }
        memcpy(prevFrame, frame, frameBufSize);
    } else {
        for(uint16_t i = 0; i < frameBufSize; i++) {
            x = (i / DISPLAY_FRAME_HEIGHT_BYTES);
            y_byte = i % DISPLAY_FRAME_HEIGHT_BYTES;
            for (uint8_t y_bit = 0; y_bit < 7; y_bit++) {
                y = (y_byte * 8) + y_bit;
                p = x / CONFIG_BROSE_PANEL_WIDTH;
                if (p != prev_p) {
                    //ESP_LOGV(LOG_TAG, "Selecting panel %d", p);
                    //display_select_panel(p);
                    prev_p = p;
                }
                display_select_color(BUFFER_VAL(frame, x, y));
                display_select_column(x % CONFIG_BROSE_PANEL_WIDTH);
                display_select_row(y);
                ESP_LOGV(LOG_TAG, "frame[%d, %d, %d] = %d", p, x, y, frame[i]);
                display_flip();
            }
        }
        if(display_dirty && prevFrame) memcpy(prevFrame, frame, frameBufSize);
        display_dirty = 0;
    }
    display_deselect();
}

#endif