#pragma once

#include "esp_system.h"
#include "nvs.h"

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
void display_buffers_to_out_buf(uint8_t* outBuf, size_t outBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render_frame(uint8_t* frame, size_t frameBufSize);
uint8_t display_get_fan_speed(uint8_t* frameBuf, uint16_t frameBufSize);