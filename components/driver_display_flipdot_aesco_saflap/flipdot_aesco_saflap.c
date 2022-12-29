/*
 * Functions for AESCO SAFLAP 12x8 pixel flipdot panels
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "flipdot_aesco_saflap.h"
#include "util_gpio.h"
#include "macros.h"

#if defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_SAFLAP)


#define LOG_TAG "FLIPDOT-AESCO-SAFLAP"

static uint8_t display_dirty = 1;

void display_init(nvs_handle_t* nvsHandle) {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_SAFLAP_DATA_IO);
    gpio_set_direction(CONFIG_SAFLAP_DATA_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_SAFLAP_DATA_IO, 0, CONFIG_SAFLAP_DATA_IO_INVERT);
    display_reset();
}

void display_shiftBit(uint8_t bit) {
    /*
     * Shift out a single bit.
     * 1 means 20µs delay, 0 means 5µs,
     * followed by a 30µs delay.
     * Thus, the maximum data rate for all ones is 20 kbit/s.
     */

    gpio_pulse_inv(CONFIG_SAFLAP_DATA_IO, 1, bit ? 20 : 5, 30, CONFIG_SAFLAP_DATA_IO_INVERT);
}

void display_shiftByte(uint8_t byte) {
    /*
     * Shift out a full byte.
     */

    //ESP_LOGD(LOG_TAG, "Shift byte 0x%02x", byte);
    for (uint8_t i = 0; i < 8; i++) {
        display_shiftBit((byte >> (7 - i)) & 1);
    }
}

// void display_setNibble(uint8_t panel, uint8_t row, uint8_t nibbleIdx, uint8_t nibbleVal) {
//     /*
//      * Set the specified nibble on the specified panel.
//      */

//     uint8_t colByte = 0x00;
//     colByte |= (1 << ((nibbleVal & (1 << 0)) ? 7 : 6));
//     colByte |= (1 << ((nibbleVal & (1 << 1)) ? 5 : 4));
//     colByte |= (1 << ((nibbleVal & (1 << 2)) ? 3 : 2));
//     colByte |= (1 << ((nibbleVal & (1 << 3)) ? 1 : 0));

//     display_shiftBit(1);            // Start Bit
//     display_shiftByte(~panel);      // Panel address
//     display_shiftByte(row << 5);    // Row address
//     display_shiftByte(nibbleIdx == 0 ? colByte : 0x00); // Column Byte 1
//     display_shiftByte(nibbleIdx == 1 ? colByte : 0x00); // Column Byte 2
//     display_shiftByte(nibbleIdx == 2 ? colByte : 0x00); // Column Byte 3
//     vTaskDelay(30 / portTICK_PERIOD_MS);
// }

// void display_setRow(uint8_t panel, uint8_t row, uint16_t cols) {
//     /*
//      * Set the specified row on the specified panel, split into 3 nibbles.
//      */

//     display_setNibble(panel, row, 0, (cols >> 8) & 0x0F);
//     display_setNibble(panel, row, 1, (cols >> 4) & 0x0F);
//     display_setNibble(panel, row, 2, cols & 0x0F);
// }

void display_reset() {
    /*
     * Reset the controller registers to get rid of any rogue bits
     */

    for (uint8_t i = 0; i < 20; i++) {
        display_shiftByte(0x00);
    }
    vTaskDelay(40 / portTICK_PERIOD_MS);
}

void display_setRowSingle(uint8_t panel, uint8_t row, uint16_t cols) {
    /*
     * Set the specified row on the specified panel, all at once.
     */

    uint8_t colByte1 = 0x00;
    colByte1 |= (1 << ((cols & (1 << 8)) ? 7 : 6));
    colByte1 |= (1 << ((cols & (1 << 9)) ? 5 : 4));
    colByte1 |= (1 << ((cols & (1 << 10)) ? 3 : 2));
    colByte1 |= (1 << ((cols & (1 << 11)) ? 1 : 0));

    uint8_t colByte2 = 0x00;
    colByte2 |= (1 << ((cols & (1 << 4)) ? 7 : 6));
    colByte2 |= (1 << ((cols & (1 << 5)) ? 5 : 4));
    colByte2 |= (1 << ((cols & (1 << 6)) ? 3 : 2));
    colByte2 |= (1 << ((cols & (1 << 7)) ? 1 : 0));

    uint8_t colByte3 = 0x00;
    colByte3 |= (1 << ((cols & (1 << 0)) ? 7 : 6));
    colByte3 |= (1 << ((cols & (1 << 1)) ? 5 : 4));
    colByte3 |= (1 << ((cols & (1 << 2)) ? 3 : 2));
    colByte3 |= (1 << ((cols & (1 << 3)) ? 1 : 0));

    display_shiftBit(1);         // Start Bit
    display_shiftByte(~panel);   // Panel address
    display_shiftByte(row << 5); // Row address
    display_shiftByte(colByte1); // Column Byte 1
    display_shiftByte(colByte2); // Column Byte 2
    display_shiftByte(colByte3); // Column Byte 3
    vTaskDelay(40 / portTICK_PERIOD_MS); // Wait for the refresh cycle to complete
}

void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    if(!display_dirty && prevFrame) {
        for (uint16_t x_base = 0; x_base < DISPLAY_FRAME_WIDTH; x_base += SAFLAP_PANEL_WIDTH) {
            for (uint16_t y = 0; y < DISPLAY_FRAME_HEIGHT; y++) {
                uint8_t panelAddr = 1 + (x_base / SAFLAP_PANEL_WIDTH) * CONFIG_SAFLAP_NUM_PANELS_Y + (y / SAFLAP_PANEL_HEIGHT);
                uint8_t panelRow = y % SAFLAP_PANEL_HEIGHT;
                uint16_t colData = 0x000;
                uint16_t prevColData = 0x000;
                uint16_t x = 0;
                for (uint16_t x_offset = 0; x_offset < SAFLAP_PANEL_WIDTH; x_offset++) {
                    x = x_base + x_offset;
                    colData |= ((uint16_t)BUFFER_VAL(frame, x, y)) << (SAFLAP_PANEL_WIDTH - x_offset - 1);
                    prevColData |= ((uint16_t)BUFFER_VAL(prevFrame, x, y)) << (SAFLAP_PANEL_WIDTH - x_offset - 1);
                }
                if (colData != prevColData) {
                    ESP_LOGD(LOG_TAG, "P%u R%u = %#03x", panelAddr, panelRow, colData);
                    display_setRowSingle(panelAddr, panelRow, colData);
                }
            }
        }
        memcpy(prevFrame, frame, frameBufSize);
    } else {
        for (uint16_t x_base = 0; x_base < DISPLAY_FRAME_WIDTH; x_base += SAFLAP_PANEL_WIDTH) {
            for (uint16_t y = 0; y < DISPLAY_FRAME_HEIGHT; y++) {
                uint8_t panelAddr = 1 + (x_base / SAFLAP_PANEL_WIDTH) * CONFIG_SAFLAP_NUM_PANELS_Y + (y / SAFLAP_PANEL_HEIGHT);
                uint8_t panelRow = y % SAFLAP_PANEL_HEIGHT;
                uint16_t colData = 0x000;
                uint16_t x = 0;
                for (uint16_t x_offset = 0; x_offset < SAFLAP_PANEL_WIDTH; x_offset++) {
                    x = x_base + x_offset;
                    colData |= ((uint16_t)BUFFER_VAL(frame, x, y)) << (SAFLAP_PANEL_WIDTH - x_offset - 1);
                }
                ESP_LOGD(LOG_TAG, "P%u R%u = 0x%03x", panelAddr, panelRow, colData);
                display_setRowSingle(panelAddr, panelRow, colData);
            }
        }
        if(display_dirty && prevFrame) memcpy(prevFrame, frame, frameBufSize);
        display_dirty = 0;
    }
}

#endif