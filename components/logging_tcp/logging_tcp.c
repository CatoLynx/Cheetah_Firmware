#include <lwip/sockets.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <errno.h>
#include "sdkconfig.h"

#include "logging_tcp.h"


#if defined(CONFIG_TCP_LOG_ENABLED)


static uint8_t numClientTasks = 0;
static tcp_log_message_t logBuf[CONFIG_TCP_LOG_BUFFER_SIZE];
static uint16_t logBufWritePtr = 0;
static uint16_t logBufReadPtr = 0;
static uint8_t logBufEmpty = 1;
static uint8_t logBufFull = 0;


void tcp_log_init() {
    esp_log_set_vprintf((vprintf_like_t)tcp_log_vprintf);
}

void tcp_log_start() {
    xTaskCreatePinnedToCore(tcp_log_server_task, "tcp_log_server", 4096, NULL, 2, NULL, 0);
}

int tcp_log_vprintf(const char* format, va_list args) {
    // vprintf(format, args);
    char* text;
    int textLen = vasprintf(&text, format, args);
    if (textLen < 0) {
        // Error
        return textLen;
    }

    tcp_log_message_t msg;
    msg.text = text;
    msg.textLen = textLen;

    tcp_log_addToBuffer(&msg);
    return textLen;
}

void tcp_log_addToBuffer(tcp_log_message_t* msg) {
    if (logBufFull) {
        free(logBuf[logBufWritePtr].text);
    }
    logBuf[logBufWritePtr] = *msg;
    logBufEmpty = 0;
    if (logBufFull) {
        logBufReadPtr++;
        if (logBufReadPtr >= CONFIG_TCP_LOG_BUFFER_SIZE) {
            logBufReadPtr = 0;
        }
    }
    logBufWritePtr++;
    if (logBufWritePtr >= CONFIG_TCP_LOG_BUFFER_SIZE) {
        logBufFull = 1;
        logBufWritePtr = 0;
    }
}

void tcp_log_server_task(void* arg) {
    struct sockaddr_in serverAddr, clientAddr;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        // ESP_LOGE(tag, "socket: %d %s", sock, strerror(errno));
		vTaskDelete(NULL);
        return;
    }

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(CONFIG_TCP_LOG_PORT);
    int ret = bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr));
    if (ret < 0) {
        // ESP_LOGE(tag, "bind: %d %s", rc, strerror(errno));
		vTaskDelete(NULL);
        return;
    }

    ret = listen(sock, CONFIG_TCP_LOG_BACKLOG);
    if (ret < 0) {
        // ESP_LOGE(tag, "listen: %d %s", rc, strerror(errno));
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSock = accept(sock, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSock < 0) {
            // ESP_LOGE(tag, "accept: %d %s", clientSock, strerror(errno));
            vTaskDelete(NULL);
            return;
        }

        int keepAlive = 1;
        int keepIdle = CONFIG_TCP_LOG_KEEPALIVE_IDLE;
        int keepInterval = CONFIG_TCP_LOG_KEEPALIVE_INTERVAL;
        int keepCount = CONFIG_TCP_LOG_KEEPALIVE_COUNT;
        setsockopt(clientSock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(clientSock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(clientSock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(clientSock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        char taskName[CONFIG_FREERTOS_MAX_TASK_NAME_LEN];
        snprintf(taskName, CONFIG_FREERTOS_MAX_TASK_NAME_LEN, "tcp_log_%u", clientSock);
#pragma GCC diagnostic pop
        xTaskCreatePinnedToCore(tcp_log_client_handler_task, taskName, 4096, (void*)clientSock, 1, NULL, 0);
    }
}

void tcp_log_client_handler_task(void* arg) {
    uint16_t bufPtr = logBufReadPtr;
    uint8_t backlogStarted = 0;

    numClientTasks++;

    // printf("Starting TCP log client handler task\n");
    // printf("Client handlers running: %u\n", numClientTasks);

    int clientSock = (int)arg;
    int bytesWritten;
    uint8_t prevRunIdle = 0;
    while (1) {
        if (prevRunIdle) vTaskDelay(1);
        if (!logBufEmpty && (!backlogStarted || bufPtr != logBufWritePtr)) {
            prevRunIdle = 0;
            backlogStarted = 1;
            tcp_log_message_t* msg = &logBuf[bufPtr];
            bufPtr++;
            if (bufPtr >= CONFIG_TCP_LOG_BUFFER_SIZE) {
                bufPtr = 0;
            }

            bytesWritten = send(clientSock, msg->text, msg->textLen, 0);
            if (bytesWritten < 0) {
                // Error
                break;
            }
        } else {
            prevRunIdle = 1;
        }
    }

    close(clientSock);
    if (numClientTasks > 0) numClientTasks--;

    // printf("Stopping TCP log client handler task\n");
    // printf("Client handlers running: %u\n", numClientTasks);
    vTaskDelete(NULL);
}

#endif