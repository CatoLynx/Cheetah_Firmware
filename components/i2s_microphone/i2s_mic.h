#pragma once

#include <stdint.h>

void i2s_mic_init();
void i2s_mic_deinit();
void i2s_mic_get_samples(float minSampleNormFactor, float maxSampleNormFactor, float maxSampleNormFactorIncrease, float maxSampleNormFactorDecrease);
void i2s_mic_get_fft_bins(float* bins, uint16_t numBins, uint8_t x_log, float lin_log_factor, float minSampleNormFactor, float maxSampleNormFactor, float maxSampleNormFactorIncrease, float maxSampleNormFactorDecrease);

void fft_test();