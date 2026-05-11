#pragma once

#include "esp_system.h"
#include "nvs.h"

#define AEG_SEL_SPI_OUT_BUF_SIZE 7
#define AEG_SEL_SPI_IN_BUF_SIZE 2
#define AEG_SEL_SPI_BUF_SIZE (AEG_SEL_SPI_OUT_BUF_SIZE) // greater of the two
#define AEG_SEL_MAX_UNITS 72

esp_err_t display_init(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units);
static void aeg_sel_splitflap_latch_input(void);
static void aeg_sel_splitflap_latch_output(void);
static void aeg_sel_splitflap_pre_transfer_cb(spi_transaction_t *t);
static void aeg_sel_splitflap_post_transfer_cb(spi_transaction_t *t);
static void aeg_sel_start_unit(uint8_t unitId);
static void aeg_sel_stop_unit(uint8_t unitId);
static esp_err_t aeg_sel_update_registers(void);
void display_update(uint8_t* unitBuf, uint8_t* prevUnitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock, uint8_t* display_framebuf_mask, uint16_t display_num_units);