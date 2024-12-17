#pragma once

#include <stdint.h>

void tpm2net_init(uint8_t* pixBuf, uint8_t* tmpBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock, size_t tmpBufSize);
void tpm2net_deinit(void);