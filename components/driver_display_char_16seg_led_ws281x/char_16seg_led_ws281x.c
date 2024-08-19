/*
 * Functions for WS281x based LED displays
 */

#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "rom/ets_sys.h"

#include "macros.h"
#include "char_16seg_led_ws281x.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "char_16seg_font.h"
#include "shaders_char.h"
#include "math.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X)


#define LOG_TAG "CH-16SEG-LED-WS281X"

spi_device_handle_t spi;
volatile uint8_t display_transferOngoing = false;
uint8_t display_currentBrightness = 255;
void* display_currentShader = NULL;
static const color_t OFF = { 0, 0, 0 };

static const uint8_t ws281x_bit_patterns[4] = {
    0x88,
	0x8E,
	0xE8,
	0xEE,
};

static uint8_t gammaLUT[256];


esp_err_t display_init(nvs_handle_t* nvsHandle) {
    /*
     * Set up all needed peripherals
     */

    // Calculate gamma lookup table
    uint16_t gammaU16;
    esp_err_t ret = nvs_get_u16(*nvsHandle, "disp_led_gamma", &gammaU16);
    if (ret != ESP_OK) gammaU16 = 100;
    if (gammaU16 == 0) gammaU16 = 100;
    double gamma = gammaU16 / 100.0;
    for (uint16_t i = 0; i < 256; i++) {
        gammaLUT[i] = round(pow(i, gamma) / pow(255, (gamma - 1)));
    }

    // Init SPI peripheral
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_16SEG_WS281X_DATA_IO,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_OUT_BUF_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 3200000UL,    // 3.2 MHz gives correct timings
        .mode = 0,                      // positive clock, sample on rising edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = display_pre_transfer_cb,
        .post_cb = display_post_transfer_cb,
    };

    #if defined(CONFIG_16SEG_WS281X_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
    #elif defined(CONFIG_16SEG_WS281X_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
    #endif
    return ESP_OK;
}

void display_pre_transfer_cb(spi_transaction_t *t) {
    display_transferOngoing = true;
}

void display_post_transfer_cb(spi_transaction_t *t) {
    display_transferOngoing = false;
}

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
esp_err_t display_set_brightness(uint8_t brightness) {
    display_currentBrightness = brightness;
    return ESP_OK;
}
#else
esp_err_t display_set_brightness(uint8_t brightness) {
    return ESP_OK;
}
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
esp_err_t display_set_shader(void* shaderData) {
    display_currentShader = shaderData;
    return ESP_OK;
}
#else
esp_err_t display_set_shader(void* shaderData) {
    return ESP_OK;
}
#endif

uint32_t display_calculateFrameBufferCharacterIndex(uint16_t charPos) {
    // Calculate the frame buffer start index for the given character position
    // Position starts at 0 in the top left corner
    uint32_t fb_index = 0;
    fb_index = DIV_CEIL(CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR, 8) * charPos;
    return fb_index;
}

void display_setLEDColor(uint8_t* frameBuf, uint16_t ledPos, color_t color) {
    color.red = gammaLUT[((uint16_t)color.red * display_currentBrightness) / 255];
    color.green = gammaLUT[((uint16_t)color.green * display_currentBrightness) / 255];
    color.blue = gammaLUT[((uint16_t)color.blue * display_currentBrightness) / 255];

    frameBuf[ledPos * 12 + 3] = ws281x_bit_patterns[(color.green >> 0) & 0x03];
    frameBuf[ledPos * 12 + 2] = ws281x_bit_patterns[(color.green >> 2) & 0x03];
    frameBuf[ledPos * 12 + 1] = ws281x_bit_patterns[(color.green >> 4) & 0x03];
    frameBuf[ledPos * 12 + 0] = ws281x_bit_patterns[(color.green >> 6) & 0x03];
    
    frameBuf[ledPos * 12 + 7] = ws281x_bit_patterns[(color.red >> 0) & 0x03];
    frameBuf[ledPos * 12 + 6] = ws281x_bit_patterns[(color.red >> 2) & 0x03];
    frameBuf[ledPos * 12 + 5] = ws281x_bit_patterns[(color.red >> 4) & 0x03];
    frameBuf[ledPos * 12 + 4] = ws281x_bit_patterns[(color.red >> 6) & 0x03];
    
    frameBuf[ledPos * 12 + 11] = ws281x_bit_patterns[(color.blue >> 0) & 0x03];
    frameBuf[ledPos * 12 + 10] = ws281x_bit_patterns[(color.blue >> 2) & 0x03];
    frameBuf[ledPos * 12 + 9] = ws281x_bit_patterns[(color.blue >> 4) & 0x03];
    frameBuf[ledPos * 12 + 8] = ws281x_bit_patterns[(color.blue >> 6) & 0x03];
}

void display_setDecimalPointAt(uint8_t* frameBuf, uint16_t charPos, uint8_t state, color_t color) {
    uint32_t fb_base_i = display_calculateFrameBufferCharacterIndex(charPos);

    if (state) {
        display_setLEDColor(&frameBuf[fb_base_i], 48, color);
    } else {
        display_setLEDColor(&frameBuf[fb_base_i], 48, OFF);
    }
}

void display_setCharDataAt(uint8_t* frameBuf, uint16_t charPos, uint16_t charData, color_t color) {
    uint32_t fb_base_i = display_calculateFrameBufferCharacterIndex(charPos);
    memset(&frameBuf[fb_base_i], 0x88, DIV_CEIL(CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR, 8));
    
    if (charData & A) {
        display_setLEDColor(&frameBuf[fb_base_i], 0, color);
        display_setLEDColor(&frameBuf[fb_base_i], 1, color);
    }
    if (charData & B) {
        display_setLEDColor(&frameBuf[fb_base_i], 2, color);
        display_setLEDColor(&frameBuf[fb_base_i], 3, color);
    }
    if (charData & C) {
        display_setLEDColor(&frameBuf[fb_base_i], 4, color);
        display_setLEDColor(&frameBuf[fb_base_i], 5, color);
        display_setLEDColor(&frameBuf[fb_base_i], 6, color);
        display_setLEDColor(&frameBuf[fb_base_i], 7, color);
    }
    if (charData & D) {
        display_setLEDColor(&frameBuf[fb_base_i], 8, color);
        display_setLEDColor(&frameBuf[fb_base_i], 9, color);
        display_setLEDColor(&frameBuf[fb_base_i], 10, color);
        display_setLEDColor(&frameBuf[fb_base_i], 11, color);
    }
    if (charData & E) {
        display_setLEDColor(&frameBuf[fb_base_i], 12, color);
        display_setLEDColor(&frameBuf[fb_base_i], 13, color);
    }
    if (charData & F) {
        display_setLEDColor(&frameBuf[fb_base_i], 14, color);
        display_setLEDColor(&frameBuf[fb_base_i], 15, color);
    }
    if (charData & G) {
        display_setLEDColor(&frameBuf[fb_base_i], 16, color);
        display_setLEDColor(&frameBuf[fb_base_i], 17, color);
        display_setLEDColor(&frameBuf[fb_base_i], 18, color);
        display_setLEDColor(&frameBuf[fb_base_i], 19, color);
    }
    if (charData & H) {
        display_setLEDColor(&frameBuf[fb_base_i], 20, color);
        display_setLEDColor(&frameBuf[fb_base_i], 21, color);
        display_setLEDColor(&frameBuf[fb_base_i], 22, color);
        display_setLEDColor(&frameBuf[fb_base_i], 23, color);
    }
    if (charData & K) {
        display_setLEDColor(&frameBuf[fb_base_i], 24, color);
        display_setLEDColor(&frameBuf[fb_base_i], 25, color);
        display_setLEDColor(&frameBuf[fb_base_i], 26, color);
    }
    if (charData & M) {
        display_setLEDColor(&frameBuf[fb_base_i], 27, color);
        display_setLEDColor(&frameBuf[fb_base_i], 28, color);
        display_setLEDColor(&frameBuf[fb_base_i], 29, color);
        display_setLEDColor(&frameBuf[fb_base_i], 30, color);
    }
    if (charData & N) {
        display_setLEDColor(&frameBuf[fb_base_i], 31, color);
        display_setLEDColor(&frameBuf[fb_base_i], 32, color);
        display_setLEDColor(&frameBuf[fb_base_i], 33, color);
    }
    if (charData & R) {
        display_setLEDColor(&frameBuf[fb_base_i], 45, color);
        display_setLEDColor(&frameBuf[fb_base_i], 46, color);
        display_setLEDColor(&frameBuf[fb_base_i], 47, color);
    }
    if (charData & S) {
        display_setLEDColor(&frameBuf[fb_base_i], 41, color);
        display_setLEDColor(&frameBuf[fb_base_i], 42, color);
        display_setLEDColor(&frameBuf[fb_base_i], 43, color);
        display_setLEDColor(&frameBuf[fb_base_i], 44, color);
    }
    if (charData & T) {
        display_setLEDColor(&frameBuf[fb_base_i], 38, color);
        display_setLEDColor(&frameBuf[fb_base_i], 39, color);
        display_setLEDColor(&frameBuf[fb_base_i], 40, color);
    }
    if (charData & U) {
        display_setLEDColor(&frameBuf[fb_base_i], 36, color);
        display_setLEDColor(&frameBuf[fb_base_i], 37, color);
    }
    if (charData & P) {
        display_setLEDColor(&frameBuf[fb_base_i], 34, color);
        display_setLEDColor(&frameBuf[fb_base_i], 35, color);
    }
}

void display_charbuf_to_framebuf(uint8_t* charBuf, uint16_t* quirkFlagBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize) {
    color_t color;
    color_rgb_t calcColor_rgb;

    memset(frameBuf, 0x88, frameBufSize);

    for (uint16_t charBufIndex = 0; charBufIndex < charBufSize; charBufIndex++) {
        calcColor_rgb = shader_fromJSON(charBufIndex, charBufSize, charBuf[charBufIndex], display_currentShader);

        color.red = calcColor_rgb.r * 255;
        color.green = calcColor_rgb.g * 255;
        color.blue = calcColor_rgb.b * 255;

        if (charBuf[charBufIndex] >= char_seg_font_min && charBuf[charBufIndex] <= char_seg_font_max) {
            display_setCharDataAt(frameBuf, charBufIndex, char_16seg_font[charBuf[charBufIndex] - char_seg_font_min], color);
        }
        if (quirkFlagBuf[charBufIndex] & QUIRK_FLAG_COMBINING_FULL_STOP) {
            display_setDecimalPointAt(frameBuf, charBufIndex, 1, color);
        }
    }
}

void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    if (display_transferOngoing) return;

    ESP_LOGV(LOG_TAG, "Rendering frame:");
    //ESP_LOG_BUFFER_HEX(LOG_TAG, frame, frameBufSize);
    spi_transaction_t spi_trans = {
        .length = (uint32_t)frameBufSize * 8,
        .tx_buffer = frame,
    };
    ESP_ERROR_CHECK(spi_device_transmit(spi, &spi_trans));
    ets_delay_us(350); // Ensure reset pulse
}

#endif