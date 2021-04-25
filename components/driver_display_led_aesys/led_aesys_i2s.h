#pragma once

#include "esp_system.h"


void display_init();
void _convertBuffer(uint8_t* src);
void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);