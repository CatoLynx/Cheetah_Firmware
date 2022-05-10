#pragma once

#include "esp_system.h"

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

void display_init();
void display_pre_transfer_cb(spi_transaction_t *t);
void display_post_transfer_cb(spi_transaction_t *t);
void display_enable();
void display_disable();
void display_latch();
void display_charbuf_to_framebuf(uint8_t* charBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize);
void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);
uint8_t display_get_fan_speed(uint8_t* frameBuf, uint16_t frameBufSize);