/*
 * Functions for shift-register based LED displays
 */

#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "macros.h"
#include "char_16seg_led_spi.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "char_16seg_font.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_SPI)


#define LOG_TAG "CH-16SEG-LED-SPI"

spi_device_handle_t spi;
volatile uint8_t display_transferOngoing = false;


// TODO: Proper enable pin handling (dimming)


void display_init() {
    /*
     * Set up all needed peripherals
     */
    
    gpio_reset_pin(CONFIG_16SEG_LED_LATCH_IO);
    gpio_set_direction(CONFIG_16SEG_LED_LATCH_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_16SEG_LED_LATCH_IO, 0, CONFIG_16SEG_LED_LATCH_INV);

    #if defined(CONFIG_16SEG_LED_USE_ENABLE)
    gpio_reset_pin(CONFIG_16SEG_LED_EN_IO);
    gpio_set_direction(CONFIG_16SEG_LED_EN_IO, GPIO_MODE_OUTPUT);
    gpio_set(CONFIG_16SEG_LED_EN_IO, 0, CONFIG_16SEG_LED_EN_INV);
    #endif

    // Init SPI peripheral
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = CONFIG_16SEG_LED_DATA_IO,
        .sclk_io_num = CONFIG_16SEG_LED_CLOCK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_FRAMEBUF_SIZE
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = CONFIG_16SEG_LED_SPI_CLK_FREQ,
        .mode = 0,                      // positive clock, sample on rising edge
        .spics_io_num = -1,             // -1 = not used
        .queue_size = 1,                // max. 1 transaction in queue
        .pre_cb = display_pre_transfer_cb,
        .post_cb = display_post_transfer_cb,
    };

    #if defined(CONFIG_16SEG_LED_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &spi));
    #elif defined(CONFIG_16SEG_LED_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, 1));
    ESP_ERROR_CHECK(spi_bus_add_device(HSPI_HOST, &devcfg, &spi));
    #endif
}

void display_pre_transfer_cb(spi_transaction_t *t) {
    display_transferOngoing = true;
    display_disable();
}

void display_post_transfer_cb(spi_transaction_t *t) {
    display_latch();
    display_enable();
    display_transferOngoing = false;
}

void display_enable() {
    /*
     * Enable output
     */

    #if defined(CONFIG_16SEG_LED_USE_ENABLE)
    gpio_set(CONFIG_16SEG_LED_EN_IO, 1, CONFIG_16SEG_LED_EN_INV);
    #endif
}

void display_disable() {
    /*
     * Disable output
     */

    #if defined(CONFIG_16SEG_LED_USE_ENABLE)
    // TODO: TEMP WORKAROUND
    //gpio_set(CONFIG_16SEG_LED_EN_IO, 0, CONFIG_16SEG_LED_EN_INV);
    #endif
}

void display_latch() {
    gpio_pulse_inv(CONFIG_16SEG_LED_LATCH_IO, 1, CONFIG_16SEG_LED_LATCH_PULSE_LENGTH, CONFIG_16SEG_LED_LATCH_PULSE_LENGTH, CONFIG_16SEG_LED_LATCH_INV);
}

uint16_t display_calculateFrameBufferCharacterIndex(uint16_t charPos) {
    // Calculate the frame buffer start index for the given character position
    // Position starts at 0 in the top left corner
    uint16_t fb_index = 0;
    fb_index = CONFIG_DISPLAY_FONT_BYTES_PER_CHAR * (DISPLAY_CHARBUF_SIZE - charPos - 1);
    #if defined(CONFIG_16SEG_LED_USE_DP_DATA)
        fb_index += (DISPLAY_CHARBUF_SIZE - charPos - 1) / CONFIG_16SEG_LED_DP_DATA_INTERVAL;
    #endif
    return fb_index;
}

#if defined(CONFIG_16SEG_LED_USE_DP_DATA)
uint16_t display_calculateFrameBufferDecimalPointIndex(uint16_t charPos) {
    // Calculate the frame buffer start index for the decimal point for the given character position
    // Position starts at 0 in the top left corner
    return (((DISPLAY_CHARBUF_SIZE - charPos - 1) / CONFIG_16SEG_LED_DP_DATA_INTERVAL) * (CONFIG_DISPLAY_FONT_BYTES_PER_CHAR * CONFIG_16SEG_LED_DP_DATA_INTERVAL + 1)) + (CONFIG_DISPLAY_FONT_BYTES_PER_CHAR * CONFIG_16SEG_LED_DP_DATA_INTERVAL);
}

uint8_t display_calculateDecimalPointSubIndex(uint16_t charPos) {
    // Calculate the bit position for the decimal point for the given character position
    // Position starts at 0 in the top left corner
    return charPos % CONFIG_16SEG_LED_DP_DATA_INTERVAL;
}

void display_setDecimalPointAt(uint8_t* frameBuf, uint16_t charPos, uint8_t state) {
    if (state) {
        frameBuf[display_calculateFrameBufferDecimalPointIndex(charPos)] |= (1 << display_calculateDecimalPointSubIndex(charPos));
    } else {
        frameBuf[display_calculateFrameBufferDecimalPointIndex(charPos)] &= ~(1 << display_calculateDecimalPointSubIndex(charPos));
    }
}
#endif

void display_setCharDataAt(uint8_t* frameBuf, uint16_t charPos, uint16_t charData) {
    uint16_t fb_base_i = display_calculateFrameBufferCharacterIndex(charPos);
    for (uint8_t i = 0; i < CONFIG_DISPLAY_FONT_BYTES_PER_CHAR; i++) {
        frameBuf[fb_base_i + i] = (charData >> (8 * (CONFIG_DISPLAY_FONT_BYTES_PER_CHAR - i - 1))) & 0xFF;
    }
}

/*
 TODO
 - Add brightness control (also in canvas interface)
 - Display multiline text field in canvas
 */
void display_charbuf_to_framebuf(uint8_t* charBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize) {
    uint8_t prevWasLetter = 0;
    uint16_t decPointMergeCnt = 0;
    uint16_t cb_i_display = 0;

    memset(frameBuf, 0x00, frameBufSize);

    for (uint16_t cb_i_source = 0; cb_i_source < charBufSize; cb_i_source++) {
        if (charBuf[cb_i_source] == '.') {
            if (!prevWasLetter) {
                // Current char is . and previous was . too
                display_setDecimalPointAt(frameBuf, cb_i_display, 1);
            } else {
                // Current char is . but previous was a letter
                // Add the point to the previous letter and update the respective counter
                cb_i_display--;
                display_setDecimalPointAt(frameBuf, cb_i_display, 1);
                decPointMergeCnt++;
                prevWasLetter = 0;
            }
        } else {
            // Current char is a letter
            if (charBuf[cb_i_source] >= char_seg_font_min && charBuf[cb_i_source] <= char_seg_font_max) {
                display_setCharDataAt(frameBuf, cb_i_display, char_16seg_font[charBuf[cb_i_source] - char_seg_font_min]);
            }
            prevWasLetter = 1;
        }

        cb_i_display++;
    }
}

void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    if (display_transferOngoing) return;

    ESP_LOGV(LOG_TAG, "Rendering frame");
    spi_transaction_t spi_trans = {
        .length = frameBufSize * 8,
        .tx_buffer = frame,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &spi_trans));
}

uint8_t display_get_fan_speed(uint8_t* frameBuf, uint16_t frameBufSize) {
    // Calculates the required fan speed based on the number of active segments in the framebuffer
    uint32_t numActiveSegments = 0;
    for (uint16_t i = 0; i < frameBufSize; i++) {
        numActiveSegments += count_set_bits(frameBuf[i]);
    }

    // Dividing by (frameBufSize * 5) instead of * 8 to reach maximum fan speed
    // with less than literally every segment lit up
    uint16_t speed = (255 * numActiveSegments) / (frameBufSize * 5);
    if (speed > 255) speed = 255;
    return (uint8_t)speed;
}

#endif