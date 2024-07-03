#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "artnet.h"
#include "util_buffer.h"
#include "macros.h"


#define LOG_TAG "ArtNet"

static TaskHandle_t artnetTaskHandle;
static uint8_t* artnet_temp_buffer;
static size_t artnet_temp_buffer_size = 0;
static uint8_t* artnet_pixel_buffer;
static size_t artnet_pixel_buffer_size = 0;


static void artnet_task(void* arg) {
    uint8_t rx_buffer[CONFIG_ARTNET_RX_BUF_SIZE];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {
        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(CONFIG_ARTNET_PORT);
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
        ESP_LOGI(LOG_TAG, "Socket bound, port %d", CONFIG_ARTNET_PORT);

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

                artnetPacket_t packet;
                //check if ArtNet header present
                if (strncmp((const char*)rx_buffer, "Art-Net", 8) != 0) {
                    return;
                }

                packet.opcode = rx_buffer[8] | rx_buffer[9] << 8;
                if (packet.opcode == 0x5000) { // check if opcode equals Art-Net opcode
                    packet.sequence = rx_buffer[12];
                    packet.universe = rx_buffer[14] | rx_buffer[15] << 8;
                    packet.dataLength = rx_buffer[17] | rx_buffer[16] << 8;
                    packet.data = &rx_buffer[ARTNET_HEADER_LENGTH];
                }

                ESP_LOGD(LOG_TAG, "Received universe %d", packet.universe);

                // Copy partial frame to buffer
                memcpy(&artnet_temp_buffer[packet.universe * ARTNET_UNIVERSE_SIZE], packet.data, packet.dataLength);

                #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
                    #if defined(CONFIG_ARTNET_FRAME_TYPE_1BPP)
                    
                    #elif defined(CONFIG_ARTNET_FRAME_TYPE_8BPP)
                    buffer_8to1(artnet_temp_buffer, artnet_pixel_buffer, DISPLAY_FRAME_WIDTH_PIXEL, DISPLAY_FRAME_HEIGHT_PIXEL, MT_OVERWRITE);
                    #elif defined(CONFIG_ARTNET_FRAME_TYPE_24BPP)

                    #endif
                #elif defined(CONFIG_DISPLAY_PIX_BUF_TYPE_8BPP)
                    #if defined(CONFIG_ARTNET_FRAME_TYPE_1BPP)
                    
                    #elif defined(CONFIG_ARTNET_FRAME_TYPE_8BPP)

                    #elif defined(CONFIG_ARTNET_FRAME_TYPE_24BPP)

                    #endif
                #elif defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
                    #if defined(CONFIG_ARTNET_FRAME_TYPE_1BPP)
                    
                    #elif defined(CONFIG_ARTNET_FRAME_TYPE_8BPP)

                    #elif defined(CONFIG_ARTNET_FRAME_TYPE_24BPP)

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

void artnet_init(uint8_t* pixBuf, uint8_t* tmpBuf, size_t pixBufSize, size_t tmpBufSize) {
    ESP_LOGI(LOG_TAG, "Starting ArtNet receiver");
    artnet_temp_buffer = tmpBuf;
    artnet_temp_buffer_size = tmpBufSize;
    artnet_pixel_buffer = pixBuf;
    artnet_pixel_buffer_size = pixBufSize;
    xTaskCreatePinnedToCore(artnet_task, "artnet_server", 4096, NULL, 5, &artnetTaskHandle, 0);
}

void artnet_stop(void) {
    ESP_LOGI(LOG_TAG, "Stopping ArtNet receiver");
    vTaskDelete(artnetTaskHandle);
    artnet_temp_buffer = NULL;
}