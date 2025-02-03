#pragma once

#include "esp_system.h"
#include "macros.h"
#include "nvs.h"

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

#define OUTPUT_BUFFER_SIZE (DISPLAY_PIX_BUF_SIZE_1BPP * 8)

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_enable();
void display_disable();
void display_shiftBit(uint8_t byte);
void display_latch();
void display_buffers_to_out_buf(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize);
void display_update(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock);