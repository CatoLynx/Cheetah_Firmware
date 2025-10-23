/*
 * Functions for KRONE 8200 PST split-flap displays
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include "driver/uart.h"
#include "rom/ets_sys.h"

#include "macros.h"
#include "util_generic.h"
#include "util_gpio.h"
#include "util_disp_selection.h"
#include "sel_k8200_pst.h"

#if defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_8200_PST)


#define LOG_TAG "SEL-K8200-PST"

static uint8_t display_outBuf[OUTPUT_BUFFER_SIZE] = {0};

// TODO: Do something with Rx pin?


esp_err_t display_init(nvs_handle_t* nvsHandle, uint8_t* display_framebuf_mask, uint16_t* display_num_units) {
    /*
     * Set up all needed peripherals
     */

    esp_err_t ret;

    ret = display_selection_loadAndParseConfiguration(nvsHandle, display_framebuf_mask, display_num_units, LOG_TAG);
    if (ret != ESP_OK) return ret;

    uart_config_t uart_config = {
        .baud_rate = 2400,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_REF_TICK,
    };
    
    if (CONFIG_K8200_PST_SEL_TX_IO >= 0) gpio_reset_pin(CONFIG_K8200_PST_SEL_TX_IO);
    if (CONFIG_K8200_PST_SEL_RX_IO >= 0) gpio_reset_pin(CONFIG_K8200_PST_SEL_RX_IO);
    if (CONFIG_K8200_PST_SEL_NMI_IO >= 0) gpio_reset_pin(CONFIG_K8200_PST_SEL_NMI_IO);
    if (CONFIG_K8200_PST_SEL_TX_IO >= 0) gpio_set_direction(CONFIG_K8200_PST_SEL_TX_IO, GPIO_MODE_OUTPUT);
    if (CONFIG_K8200_PST_SEL_RX_IO >= 0) gpio_set_direction(CONFIG_K8200_PST_SEL_RX_IO, GPIO_MODE_INPUT);
    if (CONFIG_K8200_PST_SEL_NMI_IO >= 0) gpio_set_direction(CONFIG_K8200_PST_SEL_NMI_IO, GPIO_MODE_OUTPUT);

    ret = uart_driver_install(K8200_PST_SEL_UART, CONFIG_K8200_PST_SEL_RX_BUF_SIZE, CONFIG_K8200_PST_SEL_TX_BUF_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) return ret;
    ret = uart_param_config(K8200_PST_SEL_UART, &uart_config);
    if (ret != ESP_OK) return ret;
    ret = uart_set_pin(K8200_PST_SEL_UART, CONFIG_K8200_PST_SEL_TX_IO, CONFIG_K8200_PST_SEL_RX_IO, -1, -1);
    if (ret != ESP_OK) return ret;
    ret = uart_set_line_inverse(K8200_PST_SEL_UART, UART_SIGNAL_TXD_INV);
    if (ret != ESP_OK) return ret;

    k8200_pst_set_nmi(0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    k8200_pst_set_nmi(1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    k8200_pst_set_nmi(0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    k8200_pst_reset();

    return ESP_OK;
}

void k8200_pst_set_nmi(uint8_t state) {
    // state: 1 to assert NMI (stop units), 0 to deassert
    ESP_LOGI(LOG_TAG, "NMI=%d", state);
    if (CONFIG_K8200_PST_SEL_NMI_IO >= 0) gpio_set_level(CONFIG_K8200_PST_SEL_NMI_IO, CONFIG_K8200_PST_SEL_NMI_ACT_HIGH ? !!state : !state);
}

void k8200_pst_reset(void) {
    char command = 0x1A;
    uart_write_bytes(K8200_PST_SEL_UART, &command, 1);
}

void k8200_pst_home(void) {
    char command = 0x1B;
    uart_write_bytes(K8200_PST_SEL_UART, &command, 1);
}

void getCommandBytes_SetCode(uint8_t address, uint8_t code, uint8_t* outBuf) {
    outBuf[0] = 0x3A;
    outBuf[1] = address;
    outBuf[2] = code == 0x7F ? code : uint8_to_bcd(code); // 0x7F is always the last position (empty)
}

void display_buffers_to_out_buf(uint8_t* unitBuf, size_t unitBufSize, uint8_t* display_framebuf_mask, uint16_t display_num_units) {
    memset(display_outBuf, 0x00, OUTPUT_BUFFER_SIZE);
    uint16_t usedBufSize = display_num_units * 3 + 1;
    uint16_t bufIdx = 0;
    for (uint16_t addr = 0; addr < unitBufSize; addr++) {
        // Skip addresses that aren't present
        if (!GET_MASK(display_framebuf_mask, addr)) continue;
        getCommandBytes_SetCode(addr, unitBuf[addr], &display_outBuf[bufIdx*3]);
        bufIdx++;
    }
    display_outBuf[usedBufSize - 1] = 0x1C; // Start all units
}

esp_err_t display_render(uint16_t display_num_units) {
    esp_err_t ret = uart_wait_tx_done(K8200_PST_SEL_UART, 10 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) return ret; // If this is ESP_ERR_TIMEOUT, Tx is still ongoing

    uint16_t usedBufSize = display_num_units * 3 + 1;
    for (uint16_t i = 0; i < usedBufSize; i++) {
        uart_write_bytes(K8200_PST_SEL_UART, &display_outBuf[i], 1);
        ets_delay_us(7500);
    }
    return ESP_OK;
}

void display_update(uint8_t* unitBuf, uint8_t* prevUnitBuf, size_t unitBufSize, portMUX_TYPE* unitBufLock, uint8_t* display_framebuf_mask, uint16_t display_num_units) {
    // Nothing to do if buffer hasn't changed
    if (prevUnitBuf != NULL && memcmp(unitBuf, prevUnitBuf, unitBufSize) == 0) return;

    taskENTER_CRITICAL(unitBufLock);
    display_buffers_to_out_buf(unitBuf, unitBufSize, display_framebuf_mask, display_num_units);
    if (prevUnitBuf != NULL) memcpy(prevUnitBuf, unitBuf, unitBufSize);
    taskEXIT_CRITICAL(unitBufLock);

    esp_err_t ret = display_render(display_num_units);
    if (ret != ESP_OK) return;

    // TODO: Implement better monitoring by querying rotation status using framebuffer mask
    vTaskDelay(CONFIG_K8200_PST_SEL_ROTATION_TIMEOUT / portTICK_PERIOD_MS);
    k8200_pst_set_nmi(1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    k8200_pst_set_nmi(0);
}

#endif