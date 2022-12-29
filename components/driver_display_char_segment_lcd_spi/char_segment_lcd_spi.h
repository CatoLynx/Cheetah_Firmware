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

void display_init(nvs_handle_t* nvsHandle);
void display_pre_transfer_cb(spi_transaction_t *t);
void display_post_transfer_cb(spi_transaction_t *t);
void display_enable();
void display_disable();
void display_latch();
void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);