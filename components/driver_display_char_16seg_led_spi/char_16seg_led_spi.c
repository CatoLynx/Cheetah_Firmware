/*
 * Functions for shift-register based LED displays
 */

#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "macros.h"
#include "char_16seg_led_spi.h"
#include "util_gpio.h"
#include "char_16seg_font.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_SPI)


#define LOG_TAG "CH-16SEG-LED-SPI"

spi_device_handle_t spi;
volatile uint8_t display_transferOngoing = false;


void display_init() {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_16SEG_LED_LATCH_IO);
    gpio_set_direction(CONFIG_16SEG_LED_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_16SEG_LED_LATCH_IO, 0, CONFIG_16SEG_LED_LATCH_INV);

    #if defined(CONFIG_16SEG_LED_USE_ENABLE)
    gpio_reset_pin(CONFIG_16SEG_LED_EN_IO);
    gpio_set_direction(CONFIG_16SEG_LED_EN_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_16SEG_LED_EN_IO, 0, CONFIG_16SEG_LED_EN_INV);
    #endif

    // Init SPI peripheral
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_16SEG_LED_DATA_IO,
        .sclk_io_num = CONFIG_16SEG_LED_CLOCK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_FRAMEBUF_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_16SEG_LED_SPI_CLK_FREQ,
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
    display_disable();
}

void display_post_transfer_cb(spi_transaction_t *t) {
    display_latch();
    display_enable();
    display_transferOngoing = false;
}

void display_enable() {
    /*
     * Enable output
     */

    #if defined(CONFIG_16SEG_LED_USE_ENABLE)
    gpio_set(CONFIG_16SEG_LED_EN_IO, 1, CONFIG_16SEG_LED_EN_INV);
    #endif
}

void display_disable() {
    /*
     * Disable output
     */

    #if defined(CONFIG_16SEG_LED_USE_ENABLE)
    gpio_set(CONFIG_16SEG_LED_EN_IO, 0, CONFIG_16SEG_LED_EN_INV);
    #endif
}

void display_latch() {
    gpio_pulse_inv(CONFIG_16SEG_LED_LATCH_IO, 1, CONFIG_16SEG_LED_LATCH_PULSE_LENGTH, CONFIG_16SEG_LED_LATCH_PULSE_LENGTH, CONFIG_16SEG_LED_LATCH_INV);
}

void display_charbuf_to_framebuf(uint8_t* charBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize) {
    uint16_t fb_i = 0;
    uint8_t charData;
    memset(frameBuf, 0x00, frameBufSize);
    for (uint16_t cb_i = 0; cb_i < charBufSize; cb_i++) {
        fb_i = 2 * (charBufSize - cb_i - 1) + (charBufSize - cb_i - 1) / 8;
        for (uint8_t c = 0; c < CONFIG_DISPLAY_FONT_BYTES_PER_CHAR; c++) {
            if (fb_i + c >= frameBufSize) break;
            if (charBuf[cb_i] < char_seg_font_min || charBuf[cb_i] > char_seg_font_max) continue;
            charData = (uint8_t)(char_16seg_font[charBuf[cb_i] - char_seg_font_min] >> ((CONFIG_DISPLAY_FONT_BYTES_PER_CHAR - c - 1) * 8));
            frameBuf[fb_i + c] = charData;
        }
    }
}

void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    if (display_transferOngoing) return;

    ESP_LOGD(LOG_TAG, "Rendering frame");
    spi_transaction_t spi_trans = {
        .length = frameBufSize * 8,
        .tx_buffer = frame,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &spi_trans));
}

#endif