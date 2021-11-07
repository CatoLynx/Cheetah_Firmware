/*
 * Functions for shift-register based LED displays
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "led_shift_register.h"
#include "util_gpio.h"

#if defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER)


#define LOG_TAG "LED-SR"


void display_init() {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_DATA_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_CLK_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_LATCH_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_EN_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A0_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A1_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A2_IO);

    gpio_set_direction(CONFIG_SR_LED_MATRIX_DATA_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_CLK_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_EN_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A0_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A1_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A2_IO, GPIO_MODE_OUTPUT);

    gpio_set(CONFIG_SR_LED_MATRIX_DATA_IO, 0, CONFIG_SR_LED_MATRIX_DATA_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_CLK_IO, 0, CONFIG_SR_LED_MATRIX_CLK_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_LATCH_IO, 0, CONFIG_SR_LED_MATRIX_LATCH_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_EN_IO, 0, CONFIG_SR_LED_MATRIX_EN_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A0_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A1_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A2_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);

    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_SIZE_4BIT) || defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_SIZE_5BIT)
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A3_IO);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A3_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A3_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    #endif

    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_SIZE_5BIT)
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A4_IO);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A4_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A4_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    #endif
    
    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_USE_LATCH)
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_INV);
    #endif
}

void display_select_row(uint8_t address) {
    /*
     * Enable a row
     */

    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A0_IO, ((address & 1) != 0), CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A1_IO, ((address & 2) != 0), CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A2_IO, ((address & 4) != 0), CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_SIZE_4BIT)
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A3_IO, ((address & 8) != 0), CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    #endif

    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_USE_LATCH)
    gpio_pulse_inv(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO, 1, SR_PULSE_WIDTH_US, SR_PULSE_WIDTH_US, CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_INV);
    #endif
}

void display_enable() {
    /*
     * Enable output
     */

    gpio_set(CONFIG_SR_LED_MATRIX_EN_IO, 1, CONFIG_SR_LED_MATRIX_EN_INV);
}

void display_disable() {
    /*
     * Disable output
     */

    gpio_set(CONFIG_SR_LED_MATRIX_EN_IO, 0, CONFIG_SR_LED_MATRIX_EN_INV);
}

void display_shiftBit(uint8_t byte) {
    gpio_set(CONFIG_SR_LED_MATRIX_DATA_IO, !!byte, CONFIG_SR_LED_MATRIX_DATA_INV);
    gpio_pulse_inv(CONFIG_SR_LED_MATRIX_CLK_IO, 1, 0, 0, CONFIG_SR_LED_MATRIX_CLK_INV);
}

void display_latch() {
    gpio_pulse_inv(CONFIG_SR_LED_MATRIX_LATCH_IO, 1, CONFIG_SR_LED_MATRIX_LATCH_PULSE_LENGTH, CONFIG_SR_LED_MATRIX_LATCH_PULSE_LENGTH, CONFIG_SR_LED_MATRIX_LATCH_INV);
}

void display_render_frame_8bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    ESP_LOGV(LOG_TAG, "Rendering frame");
    for (uint8_t y = 0; y < CONFIG_DISPLAY_FRAME_HEIGHT; y++) {
        for (uint16_t i = y; i < frameBufSize; i += CONFIG_DISPLAY_FRAME_HEIGHT) {
            display_shiftBit(frame[i] > 127);
        }
        #if defined(CONFIG_SR_LED_MATRIX_EN_MODE_CONTINUOUS)
        display_disable();
        display_select_row(y);
        display_latch();
        display_enable();
        #elif defined(CONFIG_SR_LED_MATRIX_EN_MODE_PULSED)
        display_select_row(y);
        display_latch();
        gpio_pulse_inv(CONFIG_SR_LED_MATRIX_EN_IO, 1, CONFIG_SR_LED_MATRIX_EN_PULSE_LENGTH, CONFIG_SR_LED_MATRIX_EN_PULSE_LENGTH, CONFIG_SR_LED_MATRIX_EN_INV);
        #endif
    }
}

#endif