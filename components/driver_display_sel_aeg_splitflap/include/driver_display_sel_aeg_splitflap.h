#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units);
void display_update(uint8_t* unitBuf, uint8_t* prevUnitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock, uint8_t* display_framebuf_mask, uint16_t display_num_units);