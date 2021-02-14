#pragma once

#include <stdint.h>

void tpm2net_init(uint8_t* outBuf, uint8_t* tmpBuf, size_t outBufSize, size_t tmpBufSize);
void tpm2net_deinit(void);