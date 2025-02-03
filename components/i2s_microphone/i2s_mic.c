#include "i2s_mic.h"

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "rom/ets_sys.h"


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
//static uint8_t testBuf[(256 * I2S_MIC_SAMPLE_WIDTH_BYTES)];


void i2s_mic_init() {
    ESP_LOGI(LOG_TAG, "Initializing microphone");
    i2s_chan_config_t chan_cfg = {
        .id = I2S_MIC_PERIPHERAL,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 32,
        .dma_frame_num = (256 * I2S_MIC_SAMPLE_WIDTH_BYTES),
        .intr_priority = 0
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = 16000,
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

    ESP_LOGI(LOG_TAG, "Recording in 3...");
    ets_delay_us(1000000);
    ESP_LOGI(LOG_TAG, "Recording in 2...");
    ets_delay_us(1000000);
    ESP_LOGI(LOG_TAG, "Recording in 1...");
    ets_delay_us(1000000);

    i2s_channel_init_std_mode(rx_handle, &std_cfg);
    i2s_channel_enable(rx_handle);
    
    size_t bytesRead = 0;
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
    fclose(file);
}

void i2s_mic_deinit() {
    i2s_channel_disable(rx_handle);
    i2s_del_channel(rx_handle);
}