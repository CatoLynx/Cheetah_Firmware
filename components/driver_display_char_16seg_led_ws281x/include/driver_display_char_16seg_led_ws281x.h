#pragma once

#include <stdint.h>
#include "nvs.h"

esp_err_t display_init(nvs_handle_t* nvsHandle);
esp_err_t display_set_brightness(uint8_t brightness);
esp_err_t display_set_shader(void* shaderData);
void display_charbuf_to_framebuf(uint8_t* charBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize);
void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);
uint8_t display_get_fan_speed(uint8_t* frameBuf, uint16_t frameBufSize);