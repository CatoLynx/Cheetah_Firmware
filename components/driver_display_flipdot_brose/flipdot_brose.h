#pragma once

#include "esp_system.h"
#include "nvs.h"

void display_init(nvs_handle_t* nvsHandle);
void display_select_column(uint8_t address);
void display_select_row(uint8_t address);
void display_select_color(uint8_t color);
void display_deselect();
void display_flip();
void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);