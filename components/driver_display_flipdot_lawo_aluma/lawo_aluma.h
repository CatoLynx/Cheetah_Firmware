#pragma once

#include "esp_system.h"
#include "nvs.h"
#include "macros.h"

#define OUTPUT_BUFFER_SIZE (DISPLAY_FRAME_WIDTH_PIXEL * DISPLAY_FRAME_HEIGHT_PIXEL)

//TODO: Move this to Kconfig

#define PIN_CS		18	// Colour select
#define PIN_EL		21	// Enable left half panel
#define PIN_ER		19	// Enable right half panel
#define PIN_LC_N	23	// Latch column address, inverted
#define PIN_LP_N	 4	// Latch panel address, inverted
#define PIN_F		 5	// Flip impulse
#define PIN_A0		25	// Address bus bit 0
#define PIN_A1		26	// Address bus bit 1
#define PIN_A2		27	// Address bus bit 2
#define PIN_A3		14	// Address bus bit 3
#define PIN_COL_A3	33	// Column driver line decoder A3 bit
#define PIN_LED		13	// LED backlight enable

#define FLIP_PULSE_WIDTH_US     500
#define LATCH_PULSE_WIDTH_US     20
#define FLIP_PAUSE_US          1500

esp_err_t display_init(nvs_handle_t* nvsHandle);
void display_set_address(uint8_t address);
void display_select_row(uint8_t address);
void display_select_column(uint8_t address);
void display_select_panel(uint8_t address);
void display_select_color(uint8_t color);
void display_deselect();
void display_flip();
void display_set_backlight(uint8_t state);
void display_render();
void display_buffers_to_out_buf(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize);
void display_update(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock);