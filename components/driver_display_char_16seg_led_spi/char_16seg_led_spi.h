#pragma once

#include "esp_system.h"
#include "nvs.h"

#define OUTPUT_BUFFER_SIZE (DIV_CEIL(DISPLAY_CHAR_BUF_SIZE * CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR, 8))

#ifndef CONFIG_16SEG_LED_DATA_INV
#define CONFIG_16SEG_LED_DATA_INV 0
#endif

#ifndef CONFIG_16SEG_LED_CLK_INV
#define CONFIG_16SEG_LED_CLK_INV 0
#endif

#ifndef CONFIG_16SEG_LED_LATCH_INV
#define CONFIG_16SEG_LED_LATCH_INV 0
#endif

#ifndef CONFIG_16SEG_LED_EN_INV
#define CONFIG_16SEG_LED_EN_INV 0
#endif

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_pre_transfer_cb(spi_transaction_t *t);
void display_post_transfer_cb(spi_transaction_t *t);
void display_enable();
void display_disable();
void display_latch();
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_effect(void* effectData);
void display_buffers_to_out_buf(uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render();
uint8_t display_get_fan_speed();