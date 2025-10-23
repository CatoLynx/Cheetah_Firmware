#pragma once

#include "esp_system.h"
#include "nvs.h"

#if defined(CONFIG_K8200_PST_SEL_UART_0)
#define K8200_PST_SEL_UART 0
#elif defined(CONFIG_K8200_PST_SEL_UART_1)
#define K8200_PST_SEL_UART 1
#elif defined(CONFIG_K8200_PST_SEL_UART_2)
#define K8200_PST_SEL_UART 2
#endif

#ifndef CONFIG_K8200_PST_SEL_NMI_ACT_HIGH
#define CONFIG_K8200_PST_SEL_NMI_ACT_HIGH 0
#endif

#define OUTPUT_BUFFER_SIZE ((DISPLAY_UNIT_BUF_SIZE * 3) + 1)


esp_err_t display_init(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units);
void k8200_pst_set_nmi(uint8_t state);
void k8200_pst_reset(void);
void k8200_pst_home(void);
void getCommandBytes_SetCode(uint8_t address, uint8_t code, uint8_t* outBuf);
void display_buffers_to_out_buf(uint8_t* unitBuf, size_t unitBufSize, uint8_t* display_framebuf_mask, uint16_t display_num_units);
esp_err_t display_render(uint16_t display_num_units);
void display_update(uint8_t* unitBuf, uint8_t* prevUnitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock, uint8_t* display_framebuf_mask, uint16_t display_num_units);