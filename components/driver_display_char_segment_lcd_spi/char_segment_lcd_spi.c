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
#include "util_buffer.h"
#include "util_gpio.h"

#if defined(CONFIG_CSEG_LCD_FONT_GV07)
#include "char_segment_font_gv07.h"
#elif defined(CONFIG_CSEG_LCD_FONT_SAF_MOSAIC_20)
#include "char_segment_font_saf_mosaic_20.h"
#endif

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_SEG_LCD_SPI)


#define LOG_TAG "CH-SEG-LCD-SPI"

spi_device_handle_t spi;
volatile static uint8_t transferOngoing = false;
static uint8_t outBuf[OUTPUT_BUFFER_SIZE] = {0};

// true: Latch indicators (if available); false: latch LCD
volatile static bool latchIndicators = false;


esp_err_t display_init(nvs_handle_t* nvsHandle) {
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

    #if defined(CONFIG_CSEG_LCD_SEPARATE_OUTPUTS_PER_LINE)
    gpio_reset_pin(CONFIG_CSEG_LCD_LATCH_A0_IO);
    gpio_set_direction(CONFIG_CSEG_LCD_LATCH_A0_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_CSEG_LCD_LATCH_A0_IO, 0, 0);

    gpio_reset_pin(CONFIG_CSEG_LCD_LATCH_A1_IO);
    gpio_set_direction(CONFIG_CSEG_LCD_LATCH_A1_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_CSEG_LCD_LATCH_A1_IO, 0, 0);

    gpio_reset_pin(CONFIG_CSEG_LCD_LATCH_A2_IO);
    gpio_set_direction(CONFIG_CSEG_LCD_LATCH_A2_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_CSEG_LCD_LATCH_A2_IO, 0, 0);
    #endif
    
    #if defined(CONFIG_DISPLAY_LINE_FLAGS_INDICATOR_LIGHT)
    gpio_reset_pin(CONFIG_CSEG_LCD_LATCH_INDICATOR_IO);
    gpio_set_direction(CONFIG_CSEG_LCD_LATCH_INDICATOR_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_CSEG_LCD_LATCH_INDICATOR_IO, 0, CONFIG_CSEG_LCD_LATCH_INDICATOR_INV);
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
        .mosi_io_num = CONFIG_CSEG_LCD_DATA_IO,
        .sclk_io_num = CONFIG_CSEG_LCD_CLOCK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OUTPUT_BUFFER_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_CSEG_LCD_SPI_CLK_FREQ,
        .mode = CONFIG_CSEG_LCD_SPI_MODE,
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = cseg_lcd_pre_transfer_cb,
        .post_cb = cseg_lcd_post_transfer_cb,
    };
    #if defined(CONFIG_CSEG_LCD_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
    #elif defined(CONFIG_CSEG_LCD_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
    #endif
    return ESP_OK;
}

void cseg_lcd_pre_transfer_cb(spi_transaction_t *t) {
    transferOngoing = true;
    if (!latchIndicators) cseg_lcd_disable();
}

void cseg_lcd_post_transfer_cb(spi_transaction_t *t) {
    if (latchIndicators) {
        cseg_lcd_latch_indicators();
    } else {
        cseg_lcd_latch_lcd();
        cseg_lcd_enable();
    }
    transferOngoing = false;
}

void cseg_lcd_enable(void) {
    /*
     * Enable output
     */

    #if defined(CONFIG_CSEG_LCD_USE_ENABLE)
    gpio_set(CONFIG_CSEG_LCD_EN_IO, 1, CONFIG_CSEG_LCD_EN_INV);
    #endif
}

void cseg_lcd_disable(void) {
    /*
     * Disable output
     */

    #if defined(CONFIG_CSEG_LCD_USE_ENABLE)
    gpio_set(CONFIG_CSEG_LCD_EN_IO, 0, CONFIG_CSEG_LCD_EN_INV);
    #endif
}

void cseg_lcd_latch_lcd(void) {
    gpio_pulse_inv(CONFIG_CSEG_LCD_LATCH_IO, 1, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_INV);
}

void cseg_lcd_latch_indicators(void) {
    #if defined(CONFIG_DISPLAY_LINE_FLAGS_INDICATOR_LIGHT)
    gpio_pulse_inv(CONFIG_CSEG_LCD_LATCH_INDICATOR_IO, 1, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_PULSE_LENGTH, CONFIG_CSEG_LCD_LATCH_INDICATOR_INV);
    #endif
}

void cseg_lcd_select_line(uint8_t line) {
    #if defined(CONFIG_CSEG_LCD_SEPARATE_OUTPUTS_PER_LINE)
    ESP_LOGD(LOG_TAG, "Selecting line %u", line);
    gpio_set(CONFIG_CSEG_LCD_LATCH_A0_IO, !!(line & 1), 0);
    gpio_set(CONFIG_CSEG_LCD_LATCH_A1_IO, !!(line & 2), 0);
    gpio_set(CONFIG_CSEG_LCD_LATCH_A2_IO, !!(line & 4), 0);
    #endif
}

void cseg_lcd_write_indicators(uint8_t* lineFlagsBuf, size_t lineFlagsBufSize) {
    size_t numBytes = DIV_CEIL(lineFlagsBufSize, 8);
    uint8_t* indicatorData = calloc(numBytes, 1);

    for (uint16_t row = 0; row < lineFlagsBufSize; row++) {
        uint16_t index = row / 8;
        uint8_t bit = row % 8;
        if (lineFlagsBuf[row] & LINE_FLAG_INDICATOR_LIGHT) indicatorData[index] |= (1 << bit);
    }
    spi_transaction_t spi_trans = {
        .length = numBytes * 8,
        .tx_buffer = indicatorData,
    };
    latchIndicators = true;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &spi_trans));

    free(indicatorData);
}

void cseg_lcd_buffers_to_out_buf(uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize, const int8_t line) {
    // Operate on one line only. If line is negative, the "Line" is the whole character buffer.
    uint8_t* lineBuf;
    size_t lineBufSize;

    if (line < 0) {
        lineBuf = charBuf;
        lineBufSize = charBufSize;
    } else {
        lineBufSize = DISPLAY_FRAME_WIDTH_CHAR;
        lineBuf = malloc(lineBufSize);
        size_t lineStart = line * DISPLAY_FRAME_WIDTH_CHAR;
        memcpy(lineBuf, &charBuf[lineStart], lineBufSize);
    }

    const uint8_t* charData;
    memset(outBuf, 0x00, OUTPUT_BUFFER_SIZE);

    #if defined(CONFIG_CSEG_LCD_FONT_SAF_MOSAIC_20)
    // This font uses 60-bit patterns which fucks up everything, so the character data needs to be mixed together
    uint8_t* dest = outBuf;
    uint16_t lineBuf_idx;
    for (uint16_t i = 0; i < lineBufSize; i++) {
        if (dest - outBuf >= OUTPUT_BUFFER_SIZE) break;
        lineBuf_idx = lineBufSize - 1 - i;

        // Replace invalid characters with space
        if (lineBuf[lineBuf_idx] < char_seg_font_min || lineBuf[lineBuf_idx] > char_seg_font_max) lineBuf[lineBuf_idx] = ' ';
        
        charData = char_seg_font + (lineBuf[lineBuf_idx] - char_seg_font_min) * CONFIG_DISPLAY_FONT_BYTES_PER_CHAR;

        // If 1, the beginning of the pattern is not byte-aligned
        bool oddAlignment = (i % 2);
        if (oddAlignment) {
            // Copy data shifted by 4 bits and merge with existing data
            *dest = (*dest & 0xF0) | (charData[0] >> 4);
            dest++;

            // Copy the remaining bytes shifted by 4 bits
            for (uint8_t n = 1; n < CONFIG_DISPLAY_FONT_BYTES_PER_CHAR; n++) {
                *dest = (charData[n-1] << 4) | (charData[n] >> 4);
                dest++;
            }
        } else {
            // Just copy the whole thing
            memcpy(dest, charData, CONFIG_DISPLAY_FONT_BYTES_PER_CHAR);
            dest += CONFIG_DISPLAY_FONT_BYTES_PER_CHAR - 1; // -1 because the last nibble still needs to be overwritten
        }
    }
    #else
    uint16_t outBuf_idx = 0;
    for (uint16_t lineBuf_idx = 0; lineBuf_idx < lineBufSize; lineBuf_idx++) {
        outBuf_idx = (lineBufSize - lineBuf_idx - 1) * CONFIG_DISPLAY_FONT_BYTES_PER_CHAR;
        for (uint8_t c = 0; c < CONFIG_DISPLAY_FONT_BYTES_PER_CHAR; c++) {
            if (outBuf_idx + c >= OUTPUT_BUFFER_SIZE) break;
            if (lineBuf[lineBuf_idx] < char_seg_font_min || lineBuf[lineBuf_idx] > char_seg_font_max) continue;
            charData = char_seg_font + (lineBuf[lineBuf_idx] - char_seg_font_min) * CONFIG_DISPLAY_FONT_BYTES_PER_CHAR;
            outBuf[outBuf_idx + c] = charData[c];
        }
    }
    #endif

    // Free only if the lineBuf was dynamically allocated above
    if (line >= 0) free(lineBuf);
}

void cseg_lcd_render(void) {
    if (transferOngoing) return;

    ESP_LOGV(LOG_TAG, "Updating LCD");
    spi_transaction_t spi_trans = {
        .length = OUTPUT_BUFFER_SIZE * 8,
        .tx_buffer = outBuf,
    };
    latchIndicators = false;
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &spi_trans));
}

void display_update(uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize, uint8_t* lineFlagsBuf, size_t lineFlagsBufSize) {
    // TODO: double-buffer this
    cseg_lcd_write_indicators(lineFlagsBuf, lineFlagsBufSize);

    // Nothing to do if buffer hasn't changed
    if (prevTextBuf != NULL && memcmp(textBuf, prevTextBuf, textBufSize) == 0) return;

    ESP_LOGD(LOG_TAG, "Updating LCD");
    taskENTER_CRITICAL(textBufLock);
    buffer_textbuf_to_charbuf(textBuf, charBuf, quirkFlagBuf, textBufSize, charBufSize);
    if (prevTextBuf != NULL) memcpy(prevTextBuf, textBuf, textBufSize);
    taskEXIT_CRITICAL(textBufLock);

    #if defined(CONFIG_CSEG_LCD_SEPARATE_OUTPUTS_PER_LINE)
    for (uint8_t line = 0; line < DISPLAY_FRAME_HEIGHT_CHAR; line++) {
        cseg_lcd_buffers_to_out_buf(charBuf, quirkFlagBuf, charBufSize, line);
        cseg_lcd_select_line(line);
        cseg_lcd_render();
    }
    #else
    cseg_lcd_buffers_to_out_buf(charBuf, quirkFlagBuf, charBufSize, -1);
    cseg_lcd_render();
    #endif
}

#endif