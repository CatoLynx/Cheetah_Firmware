/*
 * Functions for shift-register based LED displays
 */

#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "macros.h"
#include "led_shift_register_spi.h"
#include "util_gpio.h"

#if defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER)


#define LOG_TAG "LED-SR-SPI"

volatile uint8_t display_currentRow = 0;
volatile uint8_t display_transferOngoing = false;


void display_init() {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_LATCH_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_EN_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A0_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A1_IO);
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A2_IO);

    gpio_set_direction(CONFIG_SR_LED_MATRIX_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_EN_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A0_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A1_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A2_IO, GPIO_MODE_OUTPUT);

    gpio_set(CONFIG_SR_LED_MATRIX_LATCH_IO, 0, CONFIG_SR_LED_MATRIX_LATCH_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_EN_IO, 0, CONFIG_SR_LED_MATRIX_EN_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A0_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A1_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A2_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);

    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_SIZE_4BIT)
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_A3_IO);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_A3_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_A3_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_INV);
    #endif
    
    #if defined(CONFIG_SR_LED_MATRIX_ROW_ADDR_USE_LATCH)
    gpio_reset_pin(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO);
    gpio_set_direction(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_IO, 0, CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_INV);
    #endif

    spi_device_handle_t spi;
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_FRAMEBUF_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000UL,    // 1 MHz
        .mode = 0,                      // positive clock, sample on rising edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = display_pre_transfer_cb,
        .post_cb = display_post_transfer_cb,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
}

void display_pre_transfer_cb(spi_transaction_t *t) {
    display_transferOngoing = true;
}

void display_post_transfer_cb(spi_transaction_t *t) {
    display_disable();
    display_select_row(display_currentRow);
    display_latch();
    display_enable();
    display_currentRow++;
    display_currentRow %= CONFIG_DISPLAY_FRAME_HEIGHT;
    display_transferOngoing = false;
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
    gpio_pulse_inv(CONFIG_SR_LED_MATRIX_CLK_IO, 1, SR_PULSE_WIDTH_US, SR_PULSE_WIDTH_US, CONFIG_SR_LED_MATRIX_CLK_INV);
}

void display_latch() {
    gpio_pulse_inv(CONFIG_SR_LED_MATRIX_LATCH_IO, 1, SR_PULSE_WIDTH_US, SR_PULSE_WIDTH_US, CONFIG_SR_LED_MATRIX_LATCH_INV);
}

void display_render_frame_8bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    if (display_transferOngoing) return;

    // TODO: Before continuing with SPI and shit, implement 24bpp and 1bpp frame types (in the pixel canvas as well)!
    // Allocate DMA-capable buffer
    //uint8_t* buf = heap_caps_malloc()

    ESP_LOGD(LOG_TAG, "Rendering frame");
    for (uint8_t y = 0; y < CONFIG_DISPLAY_FRAME_HEIGHT; y++) {
        for (uint16_t i = y; i < frameBufSize; i += CONFIG_DISPLAY_FRAME_HEIGHT) {
            display_shiftBit(frame[i] > 127);
        }
        display_disable();
        display_select_row(y);
        display_latch();
        display_enable();
    }
}

#endif