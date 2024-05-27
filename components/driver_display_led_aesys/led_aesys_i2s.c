/*
 * Functions for shift-register based LED displays
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "i2s_parallel.h"
#include "led_aesys_i2s.h"
#include "util_gpio.h"

#if defined(CONFIG_DISPLAY_DRIVER_LED_AESYS_I2S)


#define LOG_TAG "LED-AESYS-I2S"


#if !defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
#error "I2S aesys LED matrix driver can only be used with 1bpp frame buffer"
#endif

#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)
#define DISPLAY_OUT_BUF_SIZE DIV_CEIL(CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT, 8)
#define I2S_BUF_SIZE (DISPLAY_OUT_BUF_SIZE * 8)
uint8_t i2s_buf[I2S_BUF_SIZE];

#define CONFIG_AESYS_LED_MATRIX_DATA_INV 0
#define CONFIG_AESYS_LED_MATRIX_CLK_INV 0
#define CONFIG_AESYS_LED_MATRIX_LATCH_INV 0
#define CONFIG_AESYS_LED_MATRIX_EN_INV 1


esp_err_t display_init(nvs_handle_t* nvsHandle) {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_AESYS_LED_MATRIX_DATA_IO);
    gpio_reset_pin(CONFIG_AESYS_LED_MATRIX_CLK_IO);
    gpio_reset_pin(CONFIG_AESYS_LED_MATRIX_LATCH_IO);
    gpio_reset_pin(CONFIG_AESYS_LED_MATRIX_EN_IO);

    gpio_set_direction(CONFIG_AESYS_LED_MATRIX_DATA_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_AESYS_LED_MATRIX_CLK_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_AESYS_LED_MATRIX_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CONFIG_AESYS_LED_MATRIX_EN_IO, GPIO_MODE_OUTPUT);

    gpio_set(CONFIG_AESYS_LED_MATRIX_DATA_IO, 0, CONFIG_AESYS_LED_MATRIX_DATA_INV);
    gpio_set(CONFIG_AESYS_LED_MATRIX_CLK_IO, 0, CONFIG_AESYS_LED_MATRIX_CLK_INV);
    gpio_set(CONFIG_AESYS_LED_MATRIX_LATCH_IO, 0, CONFIG_AESYS_LED_MATRIX_LATCH_INV);
    gpio_set(CONFIG_AESYS_LED_MATRIX_EN_IO, 0, CONFIG_AESYS_LED_MATRIX_EN_INV);

    i2s_parallel_buffer_desc_t bufdesc;
    i2s_parallel_config_t cfg = {
        .gpio_bus = {CONFIG_AESYS_LED_MATRIX_DATA_IO, CONFIG_AESYS_LED_MATRIX_LATCH_IO, CONFIG_AESYS_LED_MATRIX_EN_IO},
        .gpio_clk = CONFIG_AESYS_LED_MATRIX_CLK_IO,
        .clkspeed_hz = 1 * 1000 * 1000,
        .clk_inv = true,
        .bits = I2S_PARALLEL_BITS_8,
        .buf = &bufdesc
    };

    bufdesc.memory = i2s_buf;
    bufdesc.size = I2S_BUF_SIZE;

    i2s_parallel_setup(&I2S1, &cfg);
    return ESP_OK;
}

void _convertBuffer(uint8_t* src) {
    // Convert 1bpp framebuffer to aesys I²S buffer format
    uint32_t byteIdx;
    uint8_t bitIdx;
    uint8_t byte;
    uint16_t blockIdx;      // Which 6x8 pixel block we're in, 0 is the leftmost one
    uint8_t blockOffset;    // Offset within the current block
    uint8_t blockX, blockY; // X and Y offset within the current block
    uint16_t x, y;          // X and Y positions on the display

    for (uint32_t i2sBufIdx = 0; i2sBufIdx < I2S_BUF_SIZE; i2sBufIdx++) {
        blockIdx = i2sBufIdx / 48;
        blockOffset = i2sBufIdx % 48;
        blockX = blockOffset / 8;
        blockY = blockOffset % 8;
        x = blockIdx * 6 + blockX;
        y = blockY;
        byteIdx = x * DIV_CEIL(CONFIG_DISPLAY_FRAME_HEIGHT, 8) + y / 8;
        bitIdx = y % 8;

        // Data bit
        byte = (src[byteIdx] & (1 << bitIdx)) >> bitIdx;

        // Latch bit
        if (x == CONFIG_DISPLAY_FRAME_WIDTH - 1 && y == CONFIG_DISPLAY_FRAME_HEIGHT - 1) byte |= 0x02;

        // Enable bit
        if (x > 5) byte |= 0x04;

        i2s_buf[i2sBufIdx ^ 0x02] = byte;
    }
}

void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    _convertBuffer(frame);
}

#endif