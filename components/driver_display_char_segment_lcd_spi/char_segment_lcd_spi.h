#pragma once

#include "esp_system.h"
#include "nvs.h"

#ifndef CONFIG_CSEG_LCD_DATA_INV
#define CONFIG_CSEG_LCD_DATA_INV 0
#endif

#ifndef CONFIG_CSEG_LCD_CLK_INV
#define CONFIG_CSEG_LCD_CLK_INV 0
#endif

#ifndef CONFIG_CSEG_LCD_LATCH_INV
#define CONFIG_CSEG_LCD_LATCH_INV 0
#endif

#ifndef CONFIG_CSEG_LCD_EN_INV
#define CONFIG_CSEG_LCD_EN_INV 0
#endif

#ifndef CONFIG_CSEG_LCD_LATCH_INDICATOR_INV
#define CONFIG_CSEG_LCD_LATCH_INDICATOR_INV 0
#endif

#if defined(CONFIG_CSEG_LCD_SEPARATE_OUTPUTS_PER_LINE)
#define OUTPUT_BUFFER_SIZE DIV_CEIL((CONFIG_DISPLAY_OUTPUT_WIDTH * CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR), 8)
#else
#define OUTPUT_BUFFER_SIZE DIV_CEIL((CONFIG_DISPLAY_OUTPUT_WIDTH * CONFIG_DISPLAY_OUTPUT_HEIGHT * CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR), 8)
#endif

esp_err_t display_init(nvs_handle_t* nvsHandle);
void cseg_lcd_pre_transfer_cb(spi_transaction_t *t);
void cseg_lcd_post_transfer_cb(spi_transaction_t *t);
void cseg_lcd_enable(void);
void cseg_lcd_disable(void);
void cseg_lcd_latch_lcd(void);
void cseg_lcd_latch_indicators(void);
void cseg_lcd_select_line(uint8_t line);
void cseg_lcd_write_indicators(uint8_t* lineFlagsBuf, size_t lineFlagsBufSize);
void cseg_lcd_buffers_to_out_buf(uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize, int8_t line);
void cseg_lcd_render(void);
void display_update(uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize, uint8_t* lineFlagsBuf, size_t lineFlagsBufSize);