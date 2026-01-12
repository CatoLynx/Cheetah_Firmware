#include "i2s_mic.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "rom/ets_sys.h"

#include "esp_dsp.h"
#include "util_generic.h"
#include <math.h>
#include <string.h>

#define LOG_TAG "I2S-MIC"


#if defined(CONFIG_I2S_MIC_PERIPHERAL_I2S0)
#define I2S_MIC_PERIPHERAL 0
#elif defined(CONFIG_I2S_MIC_PERIPHERAL_I2S1)
#define I2S_MIC_PERIPHERAL 1
#endif

#if defined(CONFIG_I2S_MIC_SAMPLE_WIDTH_8BIT)
#define I2S_MIC_SAMPLE_WIDTH_BYTES 1
#define I2S_DATA_BIT_WIDTH I2S_DATA_BIT_WIDTH_8BIT
#elif defined(CONFIG_I2S_MIC_SAMPLE_WIDTH_16BIT)
#define I2S_MIC_SAMPLE_WIDTH_BYTES 2
#define I2S_DATA_BIT_WIDTH I2S_DATA_BIT_WIDTH_16BIT
#elif defined(CONFIG_I2S_MIC_SAMPLE_WIDTH_24BIT)
#define I2S_MIC_SAMPLE_WIDTH_BYTES 4
#define I2S_DATA_BIT_WIDTH I2S_DATA_BIT_WIDTH_32BIT // TODO
#elif defined(CONFIG_I2S_MIC_SAMPLE_WIDTH_32BIT)
#define I2S_MIC_SAMPLE_WIDTH_BYTES 4
#define I2S_DATA_BIT_WIDTH I2S_DATA_BIT_WIDTH_32BIT
#endif

static i2s_chan_handle_t rx_handle;
static uint8_t i2s_mic_sample_buf[(256 * I2S_MIC_SAMPLE_WIDTH_BYTES)];


void i2s_mic_init() {
    ESP_LOGI(LOG_TAG, "Initializing microphone");
    i2s_chan_config_t chan_cfg = {
        .id = I2S_MIC_PERIPHERAL,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 4,
        .dma_frame_num = (256 * I2S_MIC_SAMPLE_WIDTH_BYTES),
        .intr_priority = 0
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = CONFIG_I2S_MIC_SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_384
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_MONO,
            .slot_mask = I2S_STD_SLOT_LEFT,
            .ws_width = I2S_DATA_BIT_WIDTH,
            .ws_pol = false,
            .bit_shift = true,
            .msb_right = false
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_I2S_MIC_CLK_IO,
            .ws = CONFIG_I2S_MIC_WS_IO,
            .dout = I2S_GPIO_UNUSED,
            .din = CONFIG_I2S_MIC_DATA_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        }
    };

    /*ESP_LOGI(LOG_TAG, "Recording in 3...");
    ets_delay_us(1000000);
    ESP_LOGI(LOG_TAG, "Recording in 2...");
    ets_delay_us(1000000);
    ESP_LOGI(LOG_TAG, "Recording in 1...");
    ets_delay_us(1000000);*/

    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
    
    /*size_t bytesRead = 0;
    size_t totalBytesRead = 0;
    ESP_LOGI(LOG_TAG, "Opening SPIFFS file");
    FILE* file = fopen("/spiffs/test.raw", "w");
    if (file == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to open file");
        return;
    }

    uint8_t* buf = heap_caps_malloc(450 * (256 * I2S_MIC_SAMPLE_WIDTH_BYTES), MALLOC_CAP_SPIRAM);
    for (uint16_t i = 0; i < 450; i++) {
        //ESP_LOGI(LOG_TAG, "Starting read #%u", i);
        i2s_channel_read(rx_handle, &buf[totalBytesRead], (256 * I2S_MIC_SAMPLE_WIDTH_BYTES), &bytesRead, 100);
        totalBytesRead += bytesRead;
        //ESP_LOGI(LOG_TAG, "Read %u bytes", bytesRead);
        //ESP_LOGI(LOG_TAG, "Writing to SPIFFS file");
    }

    for (uint16_t i = 0; i < 450; i++) {
        ESP_LOGI(LOG_TAG, "Writing %u bytes", (256 * I2S_MIC_SAMPLE_WIDTH_BYTES));
        fwrite(&buf[i * (256 * I2S_MIC_SAMPLE_WIDTH_BYTES)], 1, (256 * I2S_MIC_SAMPLE_WIDTH_BYTES), file);
    }
    free(buf);

    ESP_LOGI(LOG_TAG, "Closing SPIFFS file");
    fclose(file);*/
}

void i2s_mic_deinit() {
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
}

// Apparently this must be a power of two!
#define NUM_SAMPLES 1024
    
__attribute__((aligned(16)))
float window[NUM_SAMPLES];

__attribute__((aligned(16)))
float in1[NUM_SAMPLES] = { 0.0f };

__attribute__((aligned(16)))
float in2[NUM_SAMPLES] = { 0.0f };

__attribute__((aligned(16)))
float y_cf[NUM_SAMPLES * 2] = { 0.0f };

uint8_t buf[NUM_SAMPLES * 4];

float sampleNormalizationFactor = 1.0f;

void i2s_mic_get_fft_bins(float* bins, uint16_t numBins, uint8_t x_log, float lin_log_factor) {
    // ESP_LOGI(LOG_TAG, "Opening SPIFFS file");
    // FILE* file = fopen("/spiffs/test.raw", "r");
    // if (file == NULL) {
    //     ESP_LOGE(LOG_TAG, "Failed to open file");
    //     return;
    // }
    // fseek(file, 0, SEEK_END);
    // uint32_t fileSize = ftell(file);
    // rewind(file);

    // int32_t* buf32 = heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM);
    // float* bufFloat = heap_caps_malloc(fileSize, MALLOC_CAP_SPIRAM);
    // if (buf32 == NULL) {
    //     ESP_LOGE(LOG_TAG, "Failed to allocate buffer");
    //     return;
    // }

    // ESP_LOGI(LOG_TAG, "Reading file");
    // fread((uint8_t*)buf32, 1, fileSize, file);
    // ESP_LOGI(LOG_TAG, "File read");

    // ESP_LOGI(LOG_TAG, "Closing SPIFFS file");
    // fclose(file);

    // uint32_t numSamples = fileSize / 4;
    // for (uint32_t i = 0; i < numSamples; i++) {
    //     bufFloat[i] = (float)buf32[i];
    // }

    // ESP_LOGI(LOG_TAG, "bufFloat[1000] = %.2f", bufFloat[1000]);
    // //ESP_LOGI(LOG_TAG, "buf32[1000] = %08lX; buf16[1000] = %04X; buf32[1000] as 4 uint8_t = %02X %02X %02X %02X", buf32[1000], buf16[1000], ((uint8_t*)buf32)[4000], ((uint8_t*)buf32)[4001], ((uint8_t*)buf32)[4002], ((uint8_t*)buf32)[4003]);

    ESP_LOGD(LOG_TAG, "Initializing FFT");
    ESP_ERROR_CHECK(dsps_fft2r_init_fc32(NULL, CONFIG_DSP_MAX_FFT_SIZE));

    ESP_LOGD(LOG_TAG, "Generating Hann window");
    dsps_wind_hann_f32(window, NUM_SAMPLES);

    /*ESP_LOGI(LOG_TAG, "Opening SPIFFS file");
    FILE* file = fopen("/spiffs/test.raw", "w");
    if (file == NULL) {
        ESP_LOGE(LOG_TAG, "Failed to open file");
        return;
    }

    ESP_LOGI(LOG_TAG, "Allocating recording buffer");
    uint8_t* recBuf = heap_caps_malloc(100 * (NUM_SAMPLES * I2S_MIC_SAMPLE_WIDTH_BYTES), MALLOC_CAP_SPIRAM);
    uint32_t recBufOffset = 0;*/

    //for (uint32_t bufBase = 1000; bufBase < (numSamples - NUM_SAMPLES); bufBase += NUM_SAMPLES) {
    for (uint8_t n = 0; n < 1; n++) {
        /*ESP_LOGI(LOG_TAG, "Copying samples to input array");
        for (uint32_t i = 0; i < NUM_SAMPLES; i++) {
            in1[i] = bufFloat[bufBase + i];
        }*/
        //dsps_tone_gen_f32(in1, NUM_SAMPLES, 1.0, 0.16, 0);

        size_t bytesRead = 0;
        size_t totalBytesRead = 0;
        ESP_LOGD(LOG_TAG, "Capturing samples from microphone");
        for (uint16_t i = 0; i < (NUM_SAMPLES / 256); i++) {
            i2s_channel_read(rx_handle, &buf[totalBytesRead], (256 * I2S_MIC_SAMPLE_WIDTH_BYTES), &bytesRead, 100);
            totalBytesRead += bytesRead;
        }
        
        /*ESP_LOGI(LOG_TAG, "Copying samples to recording buffer");
        memcpy(&recBuf[recBufOffset], buf, NUM_SAMPLES * 4);
        recBufOffset += NUM_SAMPLES * 4;*/
        
        ESP_LOGD(LOG_TAG, "Converting samples to float");
        float max = 0.0f;
        for (uint16_t i = 0; i < NUM_SAMPLES; i++) {
            in1[i] = (float)((int32_t*)buf)[i];
            if (in1[i] > max) max = in1[i];
        }
        
        float instantaneousNormalizationFactor = 1.0f;
        if (max > 0.0f) {
            instantaneousNormalizationFactor = (float)0x7FFFFFFF / max;
        } else {
            //ESP_LOGI(LOG_TAG, "Max. sample amplitude is 0");
        }

        if (instantaneousNormalizationFactor < sampleNormalizationFactor) {
            // Inst. norm. factor smaller than prev. factor means
            // this section of audio is louder than previously.
            // Allow fast change (16th of the diff)
            float diff = (sampleNormalizationFactor - instantaneousNormalizationFactor);
            sampleNormalizationFactor -= diff / 16.0f; //fmin(diff, 1.0);
        } else if (instantaneousNormalizationFactor > sampleNormalizationFactor) {
            // Inst. norm. factor larger than prev. factor means
            // this section of audio is quieter than previously.
            // Allow slow change (512th of the diff)
            float diff = (instantaneousNormalizationFactor - sampleNormalizationFactor);
            sampleNormalizationFactor += diff / 512.0f; //fmin(diff, 0.1);
        }

        if (isinf(sampleNormalizationFactor) || isnan(sampleNormalizationFactor)) {
            if (isinf(sampleNormalizationFactor)) ESP_LOGW(LOG_TAG, "Sample normalization factor is Inf");
            if (isnan(sampleNormalizationFactor)) ESP_LOGW(LOG_TAG, "Sample normalization factor is NaN");
            ESP_LOGW(LOG_TAG, "Max. sample ampl.:  %.2f", max);
            ESP_LOGW(LOG_TAG, "Inst. norm. factor: %.2f", instantaneousNormalizationFactor);
            ESP_LOGW(LOG_TAG, "Samp. norm. factor: %.2f", sampleNormalizationFactor);
            sampleNormalizationFactor = 1.0f;
        }

        if (sampleNormalizationFactor > 500.0f) sampleNormalizationFactor = 500.0f;

        ESP_LOGD(LOG_TAG, "Normalizing samples (Factor: %.2f)", sampleNormalizationFactor);
        for (uint16_t i = 0; i < NUM_SAMPLES; i++) {
            in1[i] *= sampleNormalizationFactor;
        }

        ESP_LOGD(LOG_TAG, "Merging input arrays into complex array");
        for (uint32_t i = 0 ; i < NUM_SAMPLES ; i++) {
            y_cf[i * 2 + 0] = in1[i] * window[i];
            y_cf[i * 2 + 1] = 0;
        }

        ESP_LOGD(LOG_TAG, "Running FFT");
        ESP_ERROR_CHECK(dsps_fft2r_fc32(y_cf, NUM_SAMPLES));
        
        ESP_LOGD(LOG_TAG, "Reversing bits");
        ESP_ERROR_CHECK(dsps_bit_rev_fc32(y_cf, NUM_SAMPLES));
        
        ESP_LOGD(LOG_TAG, "Splitting complex array into output arrays");
        ESP_ERROR_CHECK(dsps_cplx2reC_fc32(y_cf, NUM_SAMPLES));
        
 
        ESP_LOGD(LOG_TAG, "Calculating power");
        for (uint32_t i = 0 ; i < NUM_SAMPLES / 2 ; i++) {
            y_cf[i] = 10 * log10f((y_cf[i * 2 + 0] * y_cf[i * 2 + 0] + y_cf[i * 2 + 1] * y_cf[i * 2 + 1]) / NUM_SAMPLES);
        }

        ESP_LOGD(LOG_TAG, "Calculating bins");
        if (x_log) {
            const float minFreq = 1; // Avoid log(0)
            const float maxFreq = (NUM_SAMPLES / 2) - 1;

            float bMin = 9999999;
            float bMax = 0;
            
            float logMin = log10(minFreq);
            float logMax = log10(maxFreq);
            float logRange = logMax - logMin;

            uint16_t prevEndIdx = 1;  // Start from the lowest meaningful index

            for (uint8_t b = 0; b < numBins; b++) {
                // Logarithmic binning with a smooth transition
                float fraction = (float)(b + 1) / numBins;  // Normalize [0, 1]
                uint16_t endIdx = pow(10, logMin + pow(fraction, lin_log_factor) * logRange);

                // Ensure the bins are contiguous and non-overlapping
                uint16_t startIdx = prevEndIdx;
                endIdx = max_i32(startIdx + 1, endIdx);  // Ensure each bin has at least 1 value

                if (endIdx >= NUM_SAMPLES / 2) endIdx = NUM_SAMPLES / 2 - 1; // Clamp to valid range

                // Log the bins for debugging
                ESP_LOGD(LOG_TAG, "Bin %d: Start = %d, End = %d", b, startIdx, endIdx);

                float bin_sum = 0.0f;
                uint16_t count = 0;
                float bin_max = 0.0f;
                
                for (uint16_t f = startIdx; f < endIdx; f++) {
                    if (y_cf[f] > bin_max) bin_max = y_cf[f];
                    bin_sum += y_cf[f];
                    count++;
                }

                //bins[b] = (count > 0) ? (bin_sum / count) : 0;  // Prevent divide-by-zero
                bins[b] = bin_max;
                if (bins[b] < bMin) bMin = bins[b];
                if (bins[b] > bMax) bMax = bins[b];

                prevEndIdx = endIdx;  // Set start of next bin
            }
        } else {
            const uint16_t binSize = ((NUM_SAMPLES / 2) / numBins);
            float bMin = 9999999;
            float bMax = 0;
            for (uint8_t b = 0; b < numBins; b++) {
                float bin_sum = 0.0f;
                for (uint8_t f = 0; f < binSize; f++) {
                    bin_sum += y_cf[b * binSize + f];
                }
                bins[b] = bin_sum / binSize;
                if (bins[b] < bMin) bMin = bins[b];
                if (bins[b] > bMax) bMax = bins[b];
            }
        }
        //ESP_LOGI(LOG_TAG, "Min = %.2f, Max = %.2f", bMin, bMax);

        //ESP_LOGI(LOG_TAG, "Rendering output");
        //dsps_view(bins, numBins, 8, 10, 150, 200, '#');
        //dsps_view(y_cf, NUM_SAMPLES / 2, 128, 15, 100, 200, '#');
    }

    /*ESP_LOGI(LOG_TAG, "Writing recording to file");
    for (uint16_t i = 0; i < 100; i++) {
        fwrite(&recBuf[i * (NUM_SAMPLES * 4)], 1, (NUM_SAMPLES * 4), file);
    }

    ESP_LOGI(LOG_TAG, "Closing SPIFFS file");
    fclose(file);

    ESP_LOGI(LOG_TAG, "Freeing buffers");
    //free(bufFloat);
    //free(buf32);
    free(recBuf);*/
}