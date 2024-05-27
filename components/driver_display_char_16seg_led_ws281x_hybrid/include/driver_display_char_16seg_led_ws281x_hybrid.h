#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_shader(void* shaderData);
void display_buffers_to_out_buf(uint8_t* outBuf, size_t outBufSize, uint8_t* pixBuf, size_t pixBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);
uint8_t display_get_fan_speed(uint8_t* frameBuf, uint16_t frameBufSize);