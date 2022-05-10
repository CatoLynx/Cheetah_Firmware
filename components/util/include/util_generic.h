#pragma once

#include "esp_system.h"

#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)

uint8_t count_set_bits(uint8_t byte);