#pragma once

#include <stdint.h>
#include "nvs.h"

void display_init(nvs_handle_t* nvsHandle);
void display_set_backlight(uint8_t state);
void display_render_frame_8bpp(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);