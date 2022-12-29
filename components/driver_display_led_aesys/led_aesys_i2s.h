#pragma once

#include "esp_system.h"
#include "nvs.h"


void display_init(nvs_handle_t* nvsHandle);
void _convertBuffer(uint8_t* src);
void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);