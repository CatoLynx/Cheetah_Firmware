/*
 * Functions for shift-register based LED displays
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "i2s_parallel.h"
#include "led_shift_register_i2s.h"
#include "util_gpio.h"

#if defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER_I2S)


#define LOG_TAG "LED-SR-I2S"


#if !defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
#error "I2S shift register LED matrix driver can only be used with 1bpp frame buffer"
#endif

#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)
#define DISPLAY_OUT_BUF_SIZE DIV_CEIL(CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT, 8)
#define I2S_BUF_SIZE (DISPLAY_OUT_BUF_SIZE * 8)
uint8_t i2s_buf[I2S_BUF_SIZE];

// Indices: Logical addresses; Values: Actual addresses
uint8_t ROW_MAP[CONFIG_SR_LED_MATRIX_NUM_ROWS] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 14, 13, 12, 11, 10, 9, 15, 16, 17};


esp_err_t display_init(nvs_handle_t* nvsHandle) {
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

    i2s_parallel_buffer_desc_t bufdesc;
    i2s_parallel_config_t cfg = {
        .gpio_bus = {CONFIG_SR_LED_MATRIX_DATA_IO, CONFIG_SR_LED_MATRIX_LATCH_IO, CONFIG_SR_LED_MATRIX_EN_IO, CONFIG_SR_LED_MATRIX_ROW_A0_IO, CONFIG_SR_LED_MATRIX_ROW_A1_IO, CONFIG_SR_LED_MATRIX_ROW_A2_IO, CONFIG_SR_LED_MATRIX_ROW_A3_IO, CONFIG_SR_LED_MATRIX_ROW_A4_IO},
        .gpio_clk = CONFIG_SR_LED_MATRIX_CLK_IO,
        .clkspeed_hz = 1 * 1000 * 1000,
        .clk_inv = false,
        .bits = I2S_PARALLEL_BITS_8,
        .buf = &bufdesc
    };

    bufdesc.memory = i2s_buf;
    bufdesc.size = I2S_BUF_SIZE;

    i2s_parallel_setup(&I2S1, &cfg);
    return ESP_OK;
}

void _convertBuffer(uint8_t* src) {
    // Convert 1bpp framebuffer to I²S buffer format
    uint32_t byteIdx;
    uint8_t bitIdx;
    uint32_t i2sBufIdx = 0;
    uint8_t byte;
    uint8_t _y;
    for (uint16_t y = 0; y < CONFIG_DISPLAY_FRAME_HEIGHT; y++) {
        for (uint16_t x = 0; x < CONFIG_DISPLAY_FRAME_WIDTH; x++) {
            byteIdx = (CONFIG_DISPLAY_FRAME_WIDTH - x - 1) * DIV_CEIL(CONFIG_DISPLAY_FRAME_HEIGHT, 8) + y / 8;
            bitIdx = y % 8;
            _y = ROW_MAP[(y - 1 + CONFIG_SR_LED_MATRIX_NUM_ROWS) % CONFIG_SR_LED_MATRIX_NUM_ROWS];

            // Data bit
            byte = (src[byteIdx] & (1 << bitIdx)) >> bitIdx;

            // Latch bit
            if (x == CONFIG_DISPLAY_FRAME_WIDTH - 1) byte |= 0x02;

            // Enable bit
            if (x < 20) byte |= 0x04;

            // Row A0 bit
            if (_y & 1) byte |= 0x08;

            // Row A1 bit
            if (_y & 2) byte |= 0x10;

            // Row A2 bit
            if (_y & 4) byte |= 0x20;

            // Row A3 bit
            if (_y < 16) byte |= 0x40;

            // Row A4 bit
            if (_y >= 8 && _y < 16) byte |= 0x80;

            i2s_buf[i2sBufIdx ^ 0x02] = byte;
            i2sBufIdx++;
        }
    }
}

void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    _convertBuffer(frame);
}

#endif