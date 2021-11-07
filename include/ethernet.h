#pragma once

#include <stdint.h>
#include "esp_event.h"


typedef struct {
    uint8_t spi_cs_gpio;
    uint8_t int_gpio;
    int8_t phy_reset_gpio;
    uint8_t phy_addr;
} spi_eth_module_config_t;


void ethernet_init(void);