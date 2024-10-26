#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_update(uint8_t* outBuf, size_t outBufSize, uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);