/*
 * Functions for WS281x based LED displays
 */

// TODO: Maybe with the RMT driver redesign in ESP-IDF v5 RMT is now viable instead of SPI?

#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "rom/ets_sys.h"

#include "char_16seg_led_ws281x_hybrid.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "char_16seg_mapping.h"
#include "shaders_char.h"
#include "transitions_pixel.h"
#include "math.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X_HYBRID)


#define LOG_TAG "CH-16SEG-LED-WS281X-HYB"

static uint8_t display_outBuf[OUTPUT_BUFFER_SIZE] = {0};
spi_device_handle_t spiUpper;
spi_device_handle_t spiLower;
volatile uint8_t display_transferOngoingUpper = false;
volatile uint8_t display_transferOngoingLower = false;
uint8_t display_currentBrightness = 255;
#if defined(CONFIG_DISPLAY_HAS_SHADERS)
void* display_currentShader = NULL;
#endif
#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
void* display_currentTransition = NULL;
uint8_t display_transitionPixBuf[DISPLAY_PIX_BUF_SIZE] = {0};
#endif
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

    // Init SPI peripherals
    spi_bus_config_t buscfgUpper = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_16SEG_WS281X_HYBRID_UPPER_DATA_IO,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = UPPER_OUT_BUF_SIZE
    };
    spi_device_interface_config_t devcfgUpper = {
        .clock_speed_hz = 3200000UL,    // 3.2 MHz gives correct timings
        .mode = 0,                      // positive clock, sample on rising edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = display_pre_transfer_cb_upper,
        .post_cb = display_post_transfer_cb_upper,
    };
    spi_bus_config_t buscfgLower = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_16SEG_WS281X_HYBRID_LOWER_DATA_IO,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LOWER_OUT_BUF_SIZE
    };
    spi_device_interface_config_t devcfgLower = {
        .clock_speed_hz = 3200000UL,    // 3.2 MHz gives correct timings
        .mode = 0,                      // positive clock, sample on rising edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = display_pre_transfer_cb_lower,
        .post_cb = display_post_transfer_cb_lower,
    };

    #if defined(CONFIG_16SEG_WS281X_HYBRID_UPPER_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfgUpper, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfgUpper, &spiUpper));
    #elif defined(CONFIG_16SEG_WS281X_HYBRID_UPPER_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfgUpper, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfgUpper, &spiUpper));
    #endif
    #if defined(CONFIG_16SEG_WS281X_HYBRID_LOWER_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfgLower, 2));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfgLower, &spiLower));
    #elif defined(CONFIG_16SEG_WS281X_HYBRID_LOWER_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfgLower, 2));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfgLower, &spiLower));
    #endif
    return ESP_OK;
}

void display_pre_transfer_cb_upper(spi_transaction_t *t) {
    display_transferOngoingUpper = true;
}

void display_post_transfer_cb_upper(spi_transaction_t *t) {
    display_transferOngoingUpper = false;
}

void display_pre_transfer_cb_lower(spi_transaction_t *t) {
    display_transferOngoingLower = true;
}

void display_post_transfer_cb_lower(spi_transaction_t *t) {
    display_transferOngoingLower = false;
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

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
esp_err_t display_set_transition(void* transitionData) {
    display_currentTransition = transitionData;
    return ESP_OK;
}
#else
esp_err_t display_set_transition(void* transitionData) {
    return ESP_OK;
}
#endif

void display_setLEDColor(uint8_t* outBuf, uint16_t ledPos, color_t color) {
    color.red = gammaLUT[((uint16_t)color.red * display_currentBrightness) / 255];
    color.green = gammaLUT[((uint16_t)color.green * display_currentBrightness) / 255];
    color.blue = gammaLUT[((uint16_t)color.blue * display_currentBrightness) / 255];

    outBuf[ledPos * BYTES_PER_LED + 3] = ws281x_bit_patterns[(color.green >> 0) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 2] = ws281x_bit_patterns[(color.green >> 2) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 1] = ws281x_bit_patterns[(color.green >> 4) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 0] = ws281x_bit_patterns[(color.green >> 6) & 0x03];
    
    outBuf[ledPos * BYTES_PER_LED + 7] = ws281x_bit_patterns[(color.red >> 0) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 6] = ws281x_bit_patterns[(color.red >> 2) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 5] = ws281x_bit_patterns[(color.red >> 4) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 4] = ws281x_bit_patterns[(color.red >> 6) & 0x03];
    
    outBuf[ledPos * BYTES_PER_LED + 11] = ws281x_bit_patterns[(color.blue >> 0) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 10] = ws281x_bit_patterns[(color.blue >> 2) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 9] = ws281x_bit_patterns[(color.blue >> 4) & 0x03];
    outBuf[ledPos * BYTES_PER_LED + 8] = ws281x_bit_patterns[(color.blue >> 6) & 0x03];
}

uint8_t display_led_in_segment(uint16_t ledPos, seg_t segment) {
    switch (segment) {
        case A1: return ledPos >= SEG_A1_START && ledPos <= SEG_A1_END;
        case A2: return ledPos >= SEG_A2_START && ledPos <= SEG_A2_END;
        case B:  return ledPos >= SEG_B_START  && ledPos <= SEG_B_END;
        case C:  return ledPos >= SEG_C_START  && ledPos <= SEG_C_END;
        case D1: return ledPos >= SEG_D1_START && ledPos <= SEG_D1_END;
        case D2: return ledPos >= SEG_D2_START && ledPos <= SEG_D2_END;
        case E:  return ledPos >= SEG_E_START  && ledPos <= SEG_E_END;
        case F:  return ledPos >= SEG_F_START  && ledPos <= SEG_F_END;
        case G1: return ledPos >= SEG_G1_START && ledPos <= SEG_G1_END;
        case G2: return ledPos >= SEG_G2_START && ledPos <= SEG_G2_END;
        case H:  return ledPos >= SEG_H_START  && ledPos <= SEG_H_END;
        case I:  return ledPos >= SEG_I_START  && ledPos <= SEG_I_END;
        case J:  return ledPos >= SEG_J_START  && ledPos <= SEG_J_END;
        case K:  return ledPos >= SEG_K_START  && ledPos <= SEG_K_END;
        case L:  return ledPos >= SEG_L_START  && ledPos <= SEG_L_END;
        case M:  return ledPos >= SEG_M_START  && ledPos <= SEG_M_END;
        case DP: return ledPos >= SEG_DP_START && ledPos <= SEG_DP_END;
        default: return 0;
    }
}

uint8_t display_led_in_char_data(uint16_t ledPos, uint32_t charData) {
    if ((charData & A1) && display_led_in_segment(ledPos, A1)) return 1;
    if ((charData & A2) && display_led_in_segment(ledPos, A2)) return 1;
    if ((charData & B)  && display_led_in_segment(ledPos, B))  return 1;
    if ((charData & C)  && display_led_in_segment(ledPos, C))  return 1;
    if ((charData & D1) && display_led_in_segment(ledPos, D1)) return 1;
    if ((charData & D2) && display_led_in_segment(ledPos, D2)) return 1;
    if ((charData & E)  && display_led_in_segment(ledPos, E))  return 1;
    if ((charData & F)  && display_led_in_segment(ledPos, F))  return 1;
    if ((charData & G1) && display_led_in_segment(ledPos, G1)) return 1;
    if ((charData & G2) && display_led_in_segment(ledPos, G2)) return 1;
    if ((charData & H)  && display_led_in_segment(ledPos, H))  return 1;
    if ((charData & I)  && display_led_in_segment(ledPos, I))  return 1;
    if ((charData & J)  && display_led_in_segment(ledPos, J))  return 1;
    if ((charData & K)  && display_led_in_segment(ledPos, K))  return 1;
    if ((charData & L)  && display_led_in_segment(ledPos, L))  return 1;
    if ((charData & M)  && display_led_in_segment(ledPos, M))  return 1;
    if ((charData & DP) && display_led_in_segment(ledPos, DP)) return 1;
    return 0;
}

void display_buffers_to_out_buf(uint8_t* pixBuf, size_t pixBufSize, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize) {
    color_t color, shaderColor;
    color_rgb_t calcColor_rgb;

    memset(display_outBuf, 0x88, OUTPUT_BUFFER_SIZE);

    for (uint16_t charPos = 0; charPos < charBufSize; charPos++) {
        uint32_t charData = 0;
        if (charBuf[charPos] >= char_seg_font_min && charBuf[charPos] <= char_seg_font_max) {
            charData = char_16seg_font[charBuf[charPos] - char_seg_font_min];
        }
        if (quirkFlagBuf[charPos] & QUIRK_FLAG_COMBINING_FULL_STOP) charData |= DP;

        #if defined(CONFIG_DISPLAY_HAS_SHADERS)
        calcColor_rgb = shader_fromJSON(charBufIndex, charBufSize, charBuf[charBufIndex], display_currentShader);
        shaderColor.red = calcColor_rgb.r * 255;
        shaderColor.green = calcColor_rgb.g * 255;
        shaderColor.blue = calcColor_rgb.b * 255;
        #endif

        for (uint16_t led = 0; led < NUM_LEDS; led++) {
            if (display_led_in_char_data(led, charData)) {
                #if defined(CONFIG_DISPLAY_HAS_SHADERS)
                color = shaderColor;
                #else
                uint16_t pixBufIndex = LED_TO_BITMAP_MAPPING[led];
                color.red = pixBuf[pixBufIndex];
                color.green = pixBuf[pixBufIndex + 1];
                color.blue = pixBuf[pixBufIndex + 2];
                #endif
            } else {
                color = OFF;
            }
            display_setLEDColor(display_outBuf, led, color);
        }
    }
}

void display_render() {
    if (display_transferOngoingUpper || display_transferOngoingLower) return;

    ESP_LOGV(LOG_TAG, "Rendering frame");
    //ESP_LOG_BUFFER_HEX(LOG_TAG, frame, outBufSize);
    spi_transaction_t spi_trans_upper = {
        .length = UPPER_OUT_BUF_SIZE * 8,
        .tx_buffer = display_outBuf,
    };
    spi_transaction_t spi_trans_lower = {
        .length = LOWER_OUT_BUF_SIZE * 8,
        .tx_buffer = &display_outBuf[CONFIG_16SEG_WS281X_HYBRID_UPPER_LOWER_SPLIT_POS * BYTES_PER_LED],
    };
    // Make sure to block for the larger of the two buffers
    if (UPPER_OUT_BUF_SIZE > LOWER_OUT_BUF_SIZE) {
        ESP_ERROR_CHECK(spi_device_queue_trans(spiLower, &spi_trans_lower, 10));
        ESP_ERROR_CHECK(spi_device_transmit(spiUpper, &spi_trans_upper));
    } else {
        ESP_ERROR_CHECK(spi_device_queue_trans(spiUpper, &spi_trans_upper, 10));
        ESP_ERROR_CHECK(spi_device_transmit(spiLower, &spi_trans_lower));
    }
    ets_delay_us(350); // Ensure reset pulse
}

void display_update(uint8_t* pixBuf, uint8_t* prevPixBuf, size_t pixBufSize, portMUX_TYPE* pixBufLock, uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize) {
    taskENTER_CRITICAL(textBufLock);
    buffer_textbuf_to_charbuf(textBuf, charBuf, quirkFlagBuf, textBufSize, charBufSize);
    if (prevTextBuf != NULL) memcpy(prevTextBuf, textBuf, textBufSize);
    taskEXIT_CRITICAL(textBufLock);
    
    taskENTER_CRITICAL(pixBufLock);
    #if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
    // TODO: This won't work. Need to a) actually render all the inbetween steps and b) work on masked bitmaps (segment mask for old and new buffers baked in)
    // otherwise we would just transition the background instead of the whole thing
    transition_fromJSON(prevPixBuf, pixBuf, display_transitionPixBuf, pixBufSize, display_currentTransition);
    display_buffers_to_out_buf(outBuf, outBufSize, display_transitionPixBuf, pixBufSize, charBuf, quirkFlagBuf, charBufSize);
    #else
    display_buffers_to_out_buf(pixBuf, pixBufSize, charBuf, quirkFlagBuf, charBufSize);
    #endif
    if (prevPixBuf != NULL) memcpy(prevPixBuf, pixBuf, pixBufSize);
    taskEXIT_CRITICAL(pixBufLock);
    display_render();
}

#endif