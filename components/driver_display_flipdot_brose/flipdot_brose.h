#pragma once

#include "esp_system.h"
#include "nvs.h"

#define OUTPUT_BUFFER_SIZE (DISPLAY_FRAME_WIDTH_PIXEL * DISPLAY_FRAME_HEIGHT_PIXEL)

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_select_column(uint8_t address);
void display_select_row(uint8_t address);
void display_select_color(uint8_t color);
void display_deselect();
void display_flip();
void display_buffers_to_out_buf(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize);
void display_render();
void display_update(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock);