#pragma once

#include "esp_system.h"
#include "nvs.h"

#define BACKLIGHT_FRAMEBUF_SIZE (CONFIG_IBIS_WS281X_NUM_LEDS * 12)

#if defined(CONFIG_IBIS_UART_0)
#define IBIS_UART 0
#elif defined(CONFIG_IBIS_UART_1)
#define IBIS_UART 1
#elif defined(CONFIG_IBIS_UART_2)
#define IBIS_UART 2
#endif

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} color_t;

esp_err_t display_init(nvs_handle_t* nvsHandle);
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_shader(void* shaderData);
void display_backlight_pre_transfer_cb(spi_transaction_t *t);
void display_backlight_post_transfer_cb(spi_transaction_t *t);
void display_setBacklightColor(color_t color);
void display_buffers_to_out_buf(uint8_t* outBuf, size_t outBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render_frame(uint8_t* outBuf, size_t outBufSize);
void display_update(uint8_t* outBuf, size_t outBufSize, uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);