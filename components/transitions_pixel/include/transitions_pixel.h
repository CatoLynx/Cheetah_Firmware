#pragma once

#include <stdint.h>
#include "util_generic.h"
#include "cJSON.h"

cJSON* transition_get_available();
void transition_instant(uint8_t* oldPixBuf, uint8_t* newPixBuf, uint8_t* outPixBuf, size_t pixBufSize);
void transition_fromJSON(uint8_t* oldPixBuf, uint8_t* newPixBuf, uint8_t* outPixBuf, size_t pixBufSize, cJSON* transitionData);