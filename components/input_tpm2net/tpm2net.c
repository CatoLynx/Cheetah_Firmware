#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "tpm2net.h"
#include "util_buffer.h"


#define LOG_TAG "tpm2.net"

static TaskHandle_t tpm2netTaskHandle;
static uint8_t* tpm2net_temp_buffer;
static size_t tpm2net_temp_buffer_size = 0;
static uint8_t* tpm2net_pixel_buffer;
static size_t tpm2net_pixel_buffer_size = 0;

uint16_t tpm2net_chunkSize = 0;


static void tpm2net_task(void* arg) {
    uint8_t rx_buffer[CONFIG_TPM2NET_RX_BUF_SIZE];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(CONFIG_TPM2NET_PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(LOG_TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(LOG_TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err < 0) {
            ESP_LOGE(LOG_TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(LOG_TAG, "Socket bound, port %d", CONFIG_TPM2NET_PORT);

        while (1) {
            ESP_LOGD(LOG_TAG, "Waiting for data");
            struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(source_addr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer), 0, (struct sockaddr *)&source_addr, &socklen);

            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(LOG_TAG, "recvfrom failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                // Get the sender's ip address as string
                if (source_addr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (source_addr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }
                ESP_LOGD(LOG_TAG, "Received %d bytes from %s", len, addr_str);

                // Skip non-tpm2.net packets
                if (rx_buffer[0] != 0x9C) continue;
                // Skip non-data messages as well
                if (rx_buffer[1] != 0xDA) continue;
                // Calculate frame length
                uint16_t packetLen = rx_buffer[2] << 8 | rx_buffer[3];
                ESP_LOGD(LOG_TAG, "Packet length: %d bytes", packetLen);

                uint8_t packetNum = rx_buffer[4];
                uint8_t numPackets = rx_buffer[5];
                ESP_LOGD(LOG_TAG, "Received packet %d of %d", packetNum, numPackets);

                if ((packetNum != numPackets || numPackets == 1) && packetLen != tpm2net_chunkSize) {
                    tpm2net_chunkSize = packetLen;
                    ESP_LOGD(LOG_TAG, "Updated chunk size: %d", tpm2net_chunkSize);
                }

                if (tpm2net_chunkSize == 0) {
                    ESP_LOGD(LOG_TAG, "Discarding packet, chunk size not yet set");
                } else {
                    // Copy partial frame to buffer
                    memcpy(&tpm2net_temp_buffer[tpm2net_chunkSize * (packetNum - 1)], &rx_buffer[6], packetLen);
                }

                #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
                    #if defined(CONFIG_TPM2NET_FRAME_TYPE_1BPP)
                    
                    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_8BPP)
                    buffer_8to1(tpm2net_temp_buffer, tpm2net_pixel_buffer, CONFIG_DISPLAY_FRAME_WIDTH, CONFIG_DISPLAY_FRAME_HEIGHT, MT_OVERWRITE);
                    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_24BPP)

                    #endif
                #elif defined(CONFIG_DISPLAY_PIX_BUF_TYPE_8BPP)
                    #if defined(CONFIG_TPM2NET_FRAME_TYPE_1BPP)
                    
                    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_8BPP)
                    memcpy(tpm2net_pixel_buffer, tpm2net_temp_buffer, tpm2net_temp_buffer_size);
                    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_24BPP)

                    #endif
                #elif defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
                    #if defined(CONFIG_TPM2NET_FRAME_TYPE_1BPP)
                    
                    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_8BPP)

                    #elif defined(CONFIG_TPM2NET_FRAME_TYPE_24BPP)
                    memcpy(tpm2net_pixel_buffer, tpm2net_temp_buffer, tpm2net_temp_buffer_size);
                    #endif
                #endif
            }
        }

        if (sock != -1) {
            ESP_LOGE(LOG_TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
    vTaskDelete(NULL);
}

void tpm2net_init(uint8_t* pixBuf, uint8_t* tmpBuf, size_t pixBufSize, size_t tmpBufSize) {
    ESP_LOGI(LOG_TAG, "Starting tpm2.net receiver");
    tpm2net_temp_buffer = tmpBuf;
    tpm2net_temp_buffer_size = tmpBufSize;
    tpm2net_pixel_buffer = pixBuf;
    tpm2net_pixel_buffer_size = pixBufSize;
    xTaskCreatePinnedToCore(tpm2net_task, "tpm2net_server", 4096, NULL, 5, &tpm2netTaskHandle, 0);
}

void tpm2net_stop(void) {
    ESP_LOGI(LOG_TAG, "Stopping tpm2.net receiver");
    vTaskDelete(tpm2netTaskHandle);
    tpm2net_temp_buffer = NULL;
}