#pragma once

#include "esp_http_server.h"
#include "nvs.h"
#include "cJSON.h"

void browser_canvas_init(httpd_handle_t* server, nvs_handle_t* nvsHandle, uint8_t* pixBuf, size_t pixBufSize, uint8_t* textBuf, size_t textBufSize, uint8_t* unitBuf, size_t unitBufSize);
void browser_canvas_stop(void);
#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
void browser_canvas_register_brightness(httpd_handle_t* server, uint8_t* brightness);
#endif
#if defined(CONFIG_DISPLAY_HAS_SHADERS)
void browser_canvas_register_shaders(httpd_handle_t* server, cJSON** shaderData);
#endif
#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
void browser_canvas_register_transitions(httpd_handle_t* server, cJSON** transitionData);
#endif