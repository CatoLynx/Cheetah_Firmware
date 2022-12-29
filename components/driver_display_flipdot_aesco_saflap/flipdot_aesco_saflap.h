#pragma once

#include "esp_system.h"
#include "nvs.h"

#define SAFLAP_PANEL_WIDTH 12
#define SAFLAP_PANEL_HEIGHT 8

#ifndef CONFIG_SAFLAP_DATA_IO_INVERT
#define CONFIG_SAFLAP_DATA_IO_INVERT 0
#endif

void display_init(nvs_handle_t* nvsHandle);
void display_shiftBit(uint8_t bit);
void display_shiftByte(uint8_t byte);
void display_setNibble(uint8_t panel, uint8_t row, uint8_t nibbleIdx, uint8_t nibbleVal);
void display_reset();
void display_setRow(uint8_t panel, uint8_t row, uint16_t cols);
void display_setRowSingle(uint8_t panel, uint8_t row, uint16_t cols);
void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);