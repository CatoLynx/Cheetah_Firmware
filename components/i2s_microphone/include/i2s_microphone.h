#pragma once

#include <stdint.h>

void i2s_mic_init();
void i2s_mic_deinit();
void i2s_mic_get_fft_bins(float* bins, uint16_t numBins, uint8_t x_log, float lin_log_factor);

void fft_test();