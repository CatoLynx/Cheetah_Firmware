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
void display_buffers_to_out_buf(uint8_t* outBuf, size_t outBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);
void display_render_frame(uint8_t* frame, uint16_t frameBufSize);
void display_update(uint8_t* outBuf, size_t outBufSize, uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize);