#pragma once

#include <stdint.h>
#include "cJSON.h"

cJSON* effect_get_available();
uint8_t effect_glitches(uint8_t* charBuf, size_t charBufSize, uint8_t character_percentage, uint16_t duration_avg_ms, uint16_t duration_spread_ms, uint16_t interval_avg_ms, uint16_t interval_spread_ms);
uint8_t effect_fromJSON(uint8_t* charBuf, size_t charBufSize, cJSON* effectData);