/*
 * Functions for KRONE 9000 split-flap displays
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "driver/uart.h"

#include "macros.h"
#include "util_gpio.h"
#include "char_k9000.h"

#if defined(CONFIG_DISPLAY_DRIVER_CHAR_KRONE_9000)


#define LOG_TAG "CH-K9000"

// TODO: Do something with Rx pin?


esp_err_t display_init(nvs_handle_t* nvsHandle) {
    /*
     * Set up all needed peripherals
     */

    uart_config_t uart_config = {
        .baud_rate = 4800,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_REF_TICK,
    };
    
    if (CONFIG_K9000_TX_IO >= 0) gpio_reset_pin(CONFIG_K9000_TX_IO);
    if (CONFIG_K9000_RX_IO >= 0) gpio_reset_pin(CONFIG_K9000_RX_IO);
    if (CONFIG_K9000_TX_IO >= 0) gpio_set_direction(CONFIG_K9000_TX_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_K9000_RX_IO >= 0) gpio_set_direction(CONFIG_K9000_RX_IO, GPIO_MODE_OUTPUT);

    ESP_ERROR_CHECK(uart_driver_install(K9000_UART, CONFIG_K9000_RX_BUF_SIZE, CONFIG_K9000_TX_BUF_SIZE, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(K9000_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(K9000_UART, CONFIG_K9000_TX_IO, CONFIG_K9000_RX_IO, -1, -1));
    return ESP_OK;
}

void getCommandBytes_SetCode(uint8_t address, uint8_t code, uint8_t* outBuf) {
    uint8_t cmd_base = 0b10010000;

    if (code == 0x00) code = 0x20; // Replace null bytes with spaces

    if (address > 127) cmd_base |= 0b01000000;
    if (code    > 127) cmd_base |= 0b00100000;
    cmd_base |= 0b1000; // CMD_SET_CODE
    outBuf[0] = cmd_base;
    outBuf[1] = address & 0x7F;
    outBuf[2] = code & 0x7F;
}

void display_charbuf_to_framebuf(uint8_t* charBuf, uint16_t* quirkFlagBuf, uint8_t* frameBuf, uint16_t charBufSize, uint16_t frameBufSize) {
    // No conversion needed, display takes straight text data
    memcpy(frameBuf, charBuf, frameBufSize < charBufSize ? frameBufSize : charBufSize);
}

void display_render_frame(uint8_t* frame, uint8_t* prevFrame, uint16_t frameBufSize) {
    // Nothing to do if frame hasn't changed
    if (prevFrame != NULL && memcmp(frame, prevFrame, frameBufSize) == 0) return;

    esp_err_t ret = uart_wait_tx_done(K9000_UART, 10 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) return; // If this is ESP_ERR_TIMEOUT, Tx is still ongoing

    size_t bufSize = frameBufSize * 3 + 1; // + 1 for CMD_SET_ALL at the end
    uint8_t* buf = malloc(bufSize);

    for (uint16_t i = 0; i < frameBufSize; i++) {
        // Frame needs to be ISO-8859-1 encoded (same as UTF-8 up until 0xFF)
        getCommandBytes_SetCode(CONFIG_K9000_START_ADDR + i, frame[i], &buf[i*3]);
    }
    buf[bufSize-1] = 0b10010001; // CMD_SET_ALL

    uart_write_bytes(K9000_UART, buf, bufSize);
    free(buf);
}

#endif