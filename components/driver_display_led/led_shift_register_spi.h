#pragma once

#include "esp_system.h"

#define SR_PULSE_WIDTH_US 5

#ifndef CONFIG_SR_LED_MATRIX_DATA_INV
#define CONFIG_SR_LED_MATRIX_DATA_INV 0
#endif

#ifndef CONFIG_SR_LED_MATRIX_CLK_INV
#define CONFIG_SR_LED_MATRIX_CLK_INV 0
#endif

#ifndef CONFIG_SR_LED_MATRIX_LATCH_INV
#define CONFIG_SR_LED_MATRIX_LATCH_INV 0
#endif

#ifndef CONFIG_SR_LED_MATRIX_EN_INV
#define CONFIG_SR_LED_MATRIX_EN_INV 0
#endif

#ifndef CONFIG_SR_LED_MATRIX_ROW_ADDR_INV
#define CONFIG_SR_LED_MATRIX_ROW_ADDR_INV 0
#endif

#ifndef CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_INV
#define CONFIG_SR_LED_MATRIX_ROW_ADDR_LATCH_INV 0
#endif

void display_init();
void display_enable();
void display_disable();
void display_shiftBit(uint8_t byte);
void display_latch();
void display_render_frame_8bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);