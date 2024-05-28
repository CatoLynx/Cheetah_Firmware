#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_shader(void* shaderData);
uint8_t display_get_fan_speed(uint8_t* frameBuf, uint16_t frameBufSize);
void display_update(uint8_t* outBuf, size_t outBufSize, uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);