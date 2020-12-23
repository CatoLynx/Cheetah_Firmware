#pragma once

#include <stdint.h>

void display_setupPeripherals();
void display_setAddress(uint8_t address);
void display_selectRow(uint8_t address);
void display_selectColumn(uint8_t address);
void display_selectPanel(uint8_t address);
void display_selectColor(uint8_t color);
void display_deselect();
void display_flip();
void display_setBacklight(uint8_t state);
