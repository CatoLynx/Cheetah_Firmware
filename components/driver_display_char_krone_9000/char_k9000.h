#pragma once

#include "esp_system.h"
#include "nvs.h"

#if defined(CONFIG_K9000_UART_0)
#define K9000_UART 0
#elif defined(CONFIG_K9000_UART_1)
#define K9000_UART 1
#elif defined(CONFIG_K9000_UART_2)
#define K9000_UART 2
#endif


esp_err_t display_init(nvs_handle_t* nvsHandle);
void getCommandBytes_SetCode(uint8_t address, uint8_t code, uint8_t* outBuf);
void display_charbuf_to_framebuf(uint8_t* charBuf, uint16_t* quirkFlagBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize);
void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize);