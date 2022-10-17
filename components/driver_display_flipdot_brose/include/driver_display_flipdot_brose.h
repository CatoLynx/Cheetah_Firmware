#pragma once

#include <stdint.h>

void display_init();
void display_render_frame_1bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);