#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize, uint8_t* display_framebuf_mask, uint16_t display_num_units);