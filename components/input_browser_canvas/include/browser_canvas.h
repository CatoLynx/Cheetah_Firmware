#pragma once

#include "esp_http_server.h"
#include "cJSON.h"

void browser_canvas_init(httpd_handle_t* server, uint8_t* outBuf, size_t bufSize);
void browser_canvas_stop(void);
#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
void browser_canvas_register_brightness(httpd_handle_t* server, uint8_t* brightness);
#endif
#if defined(CONFIG_DISPLAY_HAS_SHADERS)
void browser_canvas_register_shaders(httpd_handle_t* server, cJSON** shaderData);
#endif