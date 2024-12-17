/*
 * Functions for IBIS based character displays
 * with optional WS281x backlight
 */

#include "driver/spi_master.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "rom/ets_sys.h"

#include "macros.h"
#include "char_ibis.h"
#include "util_buffer.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "shaders_char.h"
#include "math.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_IBIS)


#define LOG_TAG "CH-IBIS"

static uint8_t display_outBuf[OUTPUT_BUFFER_SIZE] = {0};
spi_device_handle_t spi;
volatile uint8_t display_backlight_transferOngoing = false;
uint8_t display_currentBrightness = 255;
void* display_currentShader = NULL;
static uint8_t backlightFrameBuf[BACKLIGHT_FRAMEBUF_SIZE] = { 0 };

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

    uart_config_t uart_config = {
        .baud_rate = 1200,
        .data_bits = UART_DATA_7_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_REF_TICK,
    };
    
    if (CONFIG_IBIS_TX_IO >= 0) gpio_reset_pin(CONFIG_IBIS_TX_IO);
    if (CONFIG_IBIS_TX_IO >= 0) gpio_set_direction(CONFIG_IBIS_TX_IO, GPIO_MODE_OUTPUT);

    ESP_ERROR_CHECK(uart_driver_install(IBIS_UART, 256, CONFIG_IBIS_TX_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(IBIS_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(IBIS_UART, CONFIG_IBIS_TX_IO, -1, -1, -1));

    #if defined(CONFIG_IBIS_TX_INVERTED)
    ESP_ERROR_CHECK(uart_set_line_inverse(IBIS_UART, UART_SIGNAL_TXD_INV));
    #endif

    #if defined(CONFIG_IBIS_HAS_WS281X_BACKLIGHT)
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
        .mosi_io_num = CONFIG_IBIS_WS281X_DATA_IO,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OUTPUT_BUFFER_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 3200000UL,    // 3.2 MHz gives correct timings
        .mode = 0,                      // positive clock, sample on rising edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = display_backlight_pre_transfer_cb,
        .post_cb = display_backlight_post_transfer_cb,
    };

    #if defined(CONFIG_IBIS_WS281X_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
    #elif defined(CONFIG_IBIS_WS281X_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
    #endif
    #endif
    return ESP_OK;
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

#if defined(CONFIG_IBIS_HAS_WS281X_BACKLIGHT)
void display_backlight_pre_transfer_cb(spi_transaction_t *t) {
    display_backlight_transferOngoing = true;
}

void display_backlight_post_transfer_cb(spi_transaction_t *t) {
    display_backlight_transferOngoing = false;
}

void display_setBacklightColor(color_t color) {
    color.red = gammaLUT[((uint16_t)color.red * display_currentBrightness) / 255];
    color.green = gammaLUT[((uint16_t)color.green * display_currentBrightness) / 255];
    color.blue = gammaLUT[((uint16_t)color.blue * display_currentBrightness) / 255];

    for (uint16_t ledPos = 0; ledPos < CONFIG_IBIS_WS281X_NUM_LEDS; ledPos++) {
        #if defined(CONFIG_IBIS_WS281X_COLOR_ORDER_GRB)
        backlightFrameBuf[ledPos * 12 + 3] = ws281x_bit_patterns[(color.green >> 0) & 0x03];
        backlightFrameBuf[ledPos * 12 + 2] = ws281x_bit_patterns[(color.green >> 2) & 0x03];
        backlightFrameBuf[ledPos * 12 + 1] = ws281x_bit_patterns[(color.green >> 4) & 0x03];
        backlightFrameBuf[ledPos * 12 + 0] = ws281x_bit_patterns[(color.green >> 6) & 0x03];
        #elif defined(CONFIG_IBIS_WS281X_COLOR_ORDER_BRG)
        backlightFrameBuf[ledPos * 12 + 3] = ws281x_bit_patterns[(color.blue >> 0) & 0x03];
        backlightFrameBuf[ledPos * 12 + 2] = ws281x_bit_patterns[(color.blue >> 2) & 0x03];
        backlightFrameBuf[ledPos * 12 + 1] = ws281x_bit_patterns[(color.blue >> 4) & 0x03];
        backlightFrameBuf[ledPos * 12 + 0] = ws281x_bit_patterns[(color.blue >> 6) & 0x03];
        #endif
        
        backlightFrameBuf[ledPos * 12 + 7] = ws281x_bit_patterns[(color.red >> 0) & 0x03];
        backlightFrameBuf[ledPos * 12 + 6] = ws281x_bit_patterns[(color.red >> 2) & 0x03];
        backlightFrameBuf[ledPos * 12 + 5] = ws281x_bit_patterns[(color.red >> 4) & 0x03];
        backlightFrameBuf[ledPos * 12 + 4] = ws281x_bit_patterns[(color.red >> 6) & 0x03];
        
        #if defined(CONFIG_IBIS_WS281X_COLOR_ORDER_GRB)
        backlightFrameBuf[ledPos * 12 + 11] = ws281x_bit_patterns[(color.blue >> 0) & 0x03];
        backlightFrameBuf[ledPos * 12 + 10] = ws281x_bit_patterns[(color.blue >> 2) & 0x03];
        backlightFrameBuf[ledPos * 12 + 9] = ws281x_bit_patterns[(color.blue >> 4) & 0x03];
        backlightFrameBuf[ledPos * 12 + 8] = ws281x_bit_patterns[(color.blue >> 6) & 0x03];
        #elif defined(CONFIG_IBIS_WS281X_COLOR_ORDER_BRG)
        backlightFrameBuf[ledPos * 12 + 11] = ws281x_bit_patterns[(color.green >> 0) & 0x03];
        backlightFrameBuf[ledPos * 12 + 10] = ws281x_bit_patterns[(color.green >> 2) & 0x03];
        backlightFrameBuf[ledPos * 12 + 9] = ws281x_bit_patterns[(color.green >> 4) & 0x03];
        backlightFrameBuf[ledPos * 12 + 8] = ws281x_bit_patterns[(color.green >> 6) & 0x03];
        #endif
    }
}
#endif

char _getIbisChar(char character) {
    // ISO-8859-1
    if (character == 0xE4) return '{'; // ä
    if (character == 0xF6) return '|'; // ö
    if (character == 0xFC) return '}'; // ü
    if (character == 0xDF) return '~'; // ß
    if (character == 0xC4) return '['; // Ä
    if (character == 0xD6) return '\\'; // Ö
    if (character == 0xDC) return ']'; // Ü
    return character;
}

uint8_t _makeChecksum(uint8_t* buf, uint16_t endPos) {
    uint8_t checksum = 0x7F;
    for (uint16_t i = 0; i < endPos; i++) {
        checksum ^= buf[i];
    }
    return checksum;
}

void display_buffers_to_out_buf(uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize) {
    #if defined(CONFIG_IBIS_TELEGRAM_TYPE_DS009)
    uint16_t pos = 0;
    // Telegram identifier
    display_outBuf[pos++] = 'v';
    // Payload
    for (uint16_t i = 0; i < charBufSize; i++) {
        if (pos >= OUTPUT_BUFFER_SIZE - 3 /*CR + checksum + NUL*/) break;
        if (charBuf[i] == 0) break;
        display_outBuf[pos++] = _getIbisChar(charBuf[i]);
    }
    display_outBuf[pos++] = '\r';
    display_outBuf[pos] = _makeChecksum(display_outBuf, pos);
    display_outBuf[++pos] = 0;
    #endif
}

void display_render() {
    ESP_LOGV(LOG_TAG, "Rendering frame:");
    //ESP_LOG_BUFFER_HEX(LOG_TAG, display_outBuf, OUTPUT_BUFFER_SIZE);

    esp_err_t ret = uart_wait_tx_done(IBIS_UART, 10 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGW(LOG_TAG, "Tx still ongoing, aborting");
        return; // If this is ESP_ERR_TIMEOUT, Tx is still ongoing
    }

    // Find first null byte
    uint16_t endPos = OUTPUT_BUFFER_SIZE - 1;
    for (uint16_t i = 0; i < OUTPUT_BUFFER_SIZE; i++) {
        if (display_outBuf[i] == 0) {
            endPos = i;
            break;
        }
    }

    // If the data ends in \r, the checksum was 0x00, so extend by one byte
    // Otherwise, the checksum will be cut off
    if ((endPos > 0) && (display_outBuf[endPos - 1] == '\r') && (endPos < (OUTPUT_BUFFER_SIZE - 1))) endPos++;

    // Send everything up to (excluding) the first null byte
    uart_write_bytes(IBIS_UART, display_outBuf, endPos);
}

void display_update(uint8_t* textBuf, uint8_t* prevTextBuf, size_t textBufSize, portMUX_TYPE* textBufLock, uint8_t* charBuf, uint16_t* quirkFlagBuf, size_t charBufSize) {
    #if defined(CONFIG_DISPLAY_HAS_SHADERS)
    // Update shader
    color_t color;
    color_rgb_t calcColor_rgb;

    memset(backlightFrameBuf, 0x88, BACKLIGHT_FRAMEBUF_SIZE);

    // Backlight color is based on first character
    calcColor_rgb = shader_fromJSON(0, charBufSize, charBuf[0], display_currentShader);
    color.red = calcColor_rgb.r * 255;
    color.green = calcColor_rgb.g * 255;
    color.blue = calcColor_rgb.b * 255;
    #if defined(CONFIG_IBIS_HAS_WS281X_BACKLIGHT)
    display_setBacklightColor(color);
    #endif
    #endif

    #if defined(CONFIG_IBIS_HAS_WS281X_BACKLIGHT)
    // Update backlight
    if (!display_backlight_transferOngoing) {
        spi_transaction_t spi_trans = {
            .length = BACKLIGHT_FRAMEBUF_SIZE * 8,
            .tx_buffer = backlightFrameBuf,
        };
        ESP_ERROR_CHECK(spi_device_transmit(spi, &spi_trans));
        ets_delay_us(350); // Ensure reset pulse
    }
    #endif

    // Nothing to do if text hasn't changed
    if (prevTextBuf != NULL && memcmp(textBuf, prevTextBuf, textBufSize) == 0) return;

    // Update display
    taskENTER_CRITICAL(textBufLock);
    buffer_textbuf_to_charbuf(textBuf, charBuf, quirkFlagBuf, textBufSize, charBufSize);
    if (prevTextBuf != NULL) memcpy(prevTextBuf, textBuf, textBufSize);
    taskEXIT_CRITICAL(textBufLock);
    display_buffers_to_out_buf(charBuf, quirkFlagBuf, charBufSize);
    display_render();
}

#endif