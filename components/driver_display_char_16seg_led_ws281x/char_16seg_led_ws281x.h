#pragma once

#include "esp_system.h"
#include "nvs.h"

#define OUTPUT_BUFFER_SIZE (DISPLAY_CHAR_BUF_SIZE * DIV_CEIL(CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR, 8))
#define WS281X_NUM_LEDS (DISPLAY_CHAR_BUF_SIZE * 49)

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_pre_transfer_cb(spi_transaction_t *t);
void display_post_transfer_cb(spi_transaction_t *t);
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_shader(void* shaderData);
esp_err_t display_set_effect(void* effectData);
uint32_t display_calculateFrameBufferCharacterIndex(uint16_t charPos);
void display_setLEDColor(uint8_t* frameBuf, uint16_t ledPos, color_t color);
void display_setDecimalPointAt(uint8_t* frameBuf, uint16_t charPos, uint8_t state, color_t color);
void display_setCharDataAt(uint8_t* frameBuf, uint16_t charPos, uint16_t charData, color_t color);
void display_buffers_to_out_buf(uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render();
void display_update(uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);