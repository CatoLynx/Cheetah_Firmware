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
#include "char_segment_lcd_spi.h"
#include "util_gpio.h"

#if defined(CONFIG_CSEG_LCD_FONT_GV07)
#include "char_segment_font_gv07.h"
#endif

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_SEG_LCD_SPI)


#define LOG_TAG "CH-SEG-LCD-SPI"

spi_device_handle_t spi;
volatile uint8_t display_transferOngoing = false;


void display_init() {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_CSEG_LCD_LATCH_IO);
    gpio_set_direction(CONFIG_CSEG_LCD_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_CSEG_LCD_LATCH_IO, 0, CONFIG_CSEG_LCD_LATCH_INV);

    #if defined(CONFIG_CSEG_LCD_USE_ENABLE)
    gpio_reset_pin(CONFIG_CSEG_LCD_EN_IO);
    gpio_set_direction(CONFIG_CSEG_LCD_EN_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_CSEG_LCD_EN_IO, 0, CONFIG_CSEG_LCD_EN_INV);
    #endif

    // Init frame clock
    ledc_timer_config_t frclk_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = CONFIG_CSEG_LCD_FRCLK_FREQ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&frclk_timer);

    ledc_channel_config_t frclk_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = 0,
        .gpio_num   = CONFIG_CSEG_LCD_FRCLK_IO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel  = LEDC_TIMER_0
    };
    ledc_channel_config(&frclk_channel);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 128);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    // Init SPI peripheral
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = 23,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_FRAMEBUF_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_CSEG_LCD_SPI_CLK_FREQ,
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

    #if defined(CONFIG_CSEG_LCD_USE_ENABLE)
    gpio_set(CONFIG_CSEG_LCD_EN_IO, 1, CONFIG_CSEG_LCD_EN_INV);
    #endif
}

void display_disable() {
    /*
     * Disable output
     */

    #if defined(CONFIG_CSEG_LCD_USE_ENABLE)
    gpio_set(CONFIG_CSEG_LCD_EN_IO, 0, CONFIG_CSEG_LCD_EN_INV);
    #endif
}

void display_latch() {
    gpio_pulse_inv(CONFIG_CSEG_LCD_LATCH_IO, 1, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_INV);
}

void display_charbuf_to_framebuf(uint8_t* charBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize) {
    uint16_t fb_i = 0;
    const uint8_t* charData;
    memset(frameBuf, 0x00, frameBufSize);
    for (uint16_t cb_i = 0; cb_i < charBufSize; cb_i++) {
        fb_i = (charBufSize - cb_i - 1) * CONFIG_DISPLAY_BYTES_PER_CHAR;
        for (uint8_t c = 0; c < CONFIG_DISPLAY_BYTES_PER_CHAR; c++) {
            if (fb_i + c >= frameBufSize) break;
            if (charBuf[cb_i] < char_seg_font_offset) continue;
            charData = char_seg_font + (charBuf[cb_i] - char_seg_font_offset) * CONFIG_DISPLAY_BYTES_PER_CHAR;
            frameBuf[fb_i + c] = charData[c];
        }
    }
}

void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    if (display_transferOngoing) return;

    ESP_LOGI(LOG_TAG, "Rendering frame");
    spi_transaction_t spi_trans = {
        .length = frameBufSize * 8,
        .tx_buffer = frame,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &spi_trans));
}

#endif