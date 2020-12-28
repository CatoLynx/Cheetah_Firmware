#pragma once

#include <stdint.h>

void tpm2net_init(uint8_t* outBuf, size_t bufSize);
void tpm2net_deinit(void);