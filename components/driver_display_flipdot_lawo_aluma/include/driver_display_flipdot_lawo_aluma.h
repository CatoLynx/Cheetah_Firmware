#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_set_backlight(uint8_t state);
void display_buffers_to_out_buf(uint8_t* outBuf, size_t outBufSize, uint8_t* pixBuf, size_t pixBufSize);
void display_update(uint8_t* outBuf, size_t outBufSize, uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize);