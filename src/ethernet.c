#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "ethernet.h"

esp_netif_t* netif_eth = NULL;

#if defined(CONFIG_ETHERNET_ENABLED)

#define LOG_TAG "Ethernet"


static void eth_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
    uint8_t mac_addr[6] = {0};
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;

    switch (event_id) {
        case ETHERNET_EVENT_CONNECTED: {
            esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
            ESP_LOGI(LOG_TAG, "Ethernet Link Up");
            ESP_LOGI(LOG_TAG, "Ethernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                    mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
            break;
        }

        case ETHERNET_EVENT_DISCONNECTED: {
            ESP_LOGI(LOG_TAG, "Ethernet Link Down");
            break;
        }

        case ETHERNET_EVENT_START: {
            ESP_LOGI(LOG_TAG, "Ethernet Started");
            break;
        }

        case ETHERNET_EVENT_STOP: {
            ESP_LOGI(LOG_TAG, "Ethernet Stopped");
            break;
        }

        default: {
            break;
        }
    }
}

static void got_ip_event_handler(void *arg, esp_event_base_t event_base,
                                 int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;

    ESP_LOGI(LOG_TAG, "Ethernet Got IP Address");
    ESP_LOGI(LOG_TAG, "ETHIP:  " IPSTR, IP2STR(&ip_info->ip));
    ESP_LOGI(LOG_TAG, "ETHMASK:" IPSTR, IP2STR(&ip_info->netmask));
    ESP_LOGI(LOG_TAG, "ETHGW:  " IPSTR, IP2STR(&ip_info->gw));
}

void ethernet_init(void) {
    esp_err_t ret = gpio_install_isr_service(0);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(LOG_TAG, "GPIO ISR handler has been already installed");
        ret = ESP_OK; // ISR handler has been already installed so no issues
    } else if(ret != ESP_OK) {
        ESP_LOGE(LOG_TAG, "GPIO ISR handler install failed");
        return;
    }

    spi_bus_config_t buscfg = {
        .miso_io_num = CONFIG_ETHERNET_MISO_IO,
        .mosi_io_num = CONFIG_ETHERNET_MOSI_IO,
        .sclk_io_num = CONFIG_ETHERNET_SCLK_IO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    #if defined(CONFIG_ETHERNET_SPI_HOST_VSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH2));
    #elif defined(CONFIG_ETHERNET_SPI_HOST_HSPI)
    ESP_ERROR_CHECK(spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH2));
    #endif

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    phy_config.phy_addr = CONFIG_ETHERNET_PHY_ADDR;
    phy_config.reset_gpio_num = CONFIG_ETHERNET_RST_IO;

    spi_device_interface_config_t spi_devcfg = {
        .mode = 0,
        .clock_speed_hz = CONFIG_ETHERNET_SPI_CLK_FREQ,
        .queue_size = 20,
        .spics_io_num = CONFIG_ETHERNET_CS_IO
    };

    #if defined(CONFIG_ETHERNET_SPI_HOST_VSPI)
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(VSPI_HOST, &spi_devcfg);
    #elif defined(CONFIG_ETHERNET_SPI_HOST_HSPI)
    eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(HSPI_HOST, &spi_devcfg);
    #endif
    w5500_config.int_gpio_num = CONFIG_ETHERNET_INT_IO;
    w5500_config.poll_period_ms = CONFIG_ETHERNET_POLLING_MS;
    esp_eth_mac_t *mac = esp_eth_mac_new_w5500(&w5500_config, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_w5500(&phy_config);

    esp_eth_handle_t eth_handle = NULL;
    esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac, phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_config_spi, &eth_handle));
    
    // Set MAC address to the ESP32's internal ETH MAC.
    // This is kinda meh, but works. The W5500 doesn't provide a MAC address of its own.
    uint8_t mac_bytes[6];
    esp_read_mac(mac_bytes, ESP_MAC_ETH);
    esp_eth_ioctl(eth_handle, ETH_CMD_S_MAC_ADDR, mac_bytes);

    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
    netif_eth = esp_netif_new(&cfg);
    esp_eth_netif_glue_handle_t eth_netif_glue = esp_eth_new_netif_glue(eth_handle);
    ESP_ERROR_CHECK(esp_netif_attach(netif_eth, eth_netif_glue));

    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_eth_start(eth_handle));
}

#endif