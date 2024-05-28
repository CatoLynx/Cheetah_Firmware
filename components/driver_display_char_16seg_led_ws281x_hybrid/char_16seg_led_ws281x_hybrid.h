#pragma once

#include "esp_system.h"
#include "nvs.h"
#include "macros.h"

#include "char_16seg_font.h"

#define BYTES_PER_LED 12
#define NUM_LEDS (DISPLAY_OUT_BUF_SIZE / BYTES_PER_LED)
#define UPPER_OUT_BUF_SIZE (BYTES_PER_LED * CONFIG_16SEG_WS281X_HYBRID_UPPER_LOWER_SPLIT_POS)
#define LOWER_OUT_BUF_SIZE (DISPLAY_OUT_BUF_SIZE - UPPER_OUT_BUF_SIZE)

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_pre_transfer_cb_upper(spi_transaction_t *t);
void display_post_transfer_cb_upper(spi_transaction_t *t);
void display_pre_transfer_cb_lower(spi_transaction_t *t);
void display_post_transfer_cb_lower(spi_transaction_t *t);
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_shader(void* shaderData);
uint32_t display_calculateFrameBufferCharacterIndex(uint16_t charPos);
void display_setLEDColor(uint8_t* frameBuf, uint16_t ledPos, color_t color);
void display_setDecimalPointAt(uint8_t* frameBuf, uint16_t charPos, uint8_t state, color_t color);
void display_setCharDataAt(uint8_t* frameBuf, uint16_t charPos, uint16_t charData, color_t color);
uint8_t display_led_in_segment(uint16_t ledPos, seg_t segment);
uint8_t display_led_in_char_data(uint16_t ledPos, uint32_t charData);
void display_buffers_to_out_buf(uint8_t* outBuf, size_t outBufSize, uint8_t* pixBuf, size_t pixBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render_frame(uint8_t* frame, uint16_t frameBufSize);
void display_update(uint8_t* outBuf, size_t outBufSize, uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);