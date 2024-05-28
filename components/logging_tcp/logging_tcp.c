#include <lwip/sockets.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
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
static SemaphoreHandle_t logBufMutex;

void tcp_log_init() {
    logBufMutex = xSemaphoreCreateMutex();
    esp_log_set_vprintf((vprintf_like_t)tcp_log_vprintf);
}

void tcp_log_start() {
    xTaskCreatePinnedToCore(tcp_log_server_task, "tcp_log_server", 4096, NULL, 2, NULL, 0);
}

int tcp_log_vprintf(const char* format, va_list args) {
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
    xSemaphoreTake(logBufMutex, portMAX_DELAY);

    if (logBufFull) {
        free(logBuf[logBufWritePtr].text);
    }
    logBuf[logBufWritePtr] = *msg;
    logBufEmpty = 0;
    logBufWritePtr++;
    if (logBufWritePtr >= CONFIG_TCP_LOG_BUFFER_SIZE) {
        logBufWritePtr = 0;
        logBufFull = 1;
    }
    if (logBufFull) {
        logBufReadPtr = logBufWritePtr;
    }

    xSemaphoreGive(logBufMutex);
}

void tcp_log_server_task(void* arg) {
    struct sockaddr_in serverAddr, clientAddr;

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
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
        vTaskDelete(NULL);
        return;
    }

    ret = listen(sock, CONFIG_TCP_LOG_BACKLOG);
    if (ret < 0) {
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSock = accept(sock, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (clientSock < 0) {
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
        xTaskCreatePinnedToCore(tcp_log_client_handler_task, taskName, 4096, (void*)(intptr_t)clientSock, 1, NULL, 0);
    }
}

void tcp_log_client_handler_task(void* arg) {
    uint16_t bufPtr = logBufReadPtr;
    uint8_t backlogStarted = 0;

    numClientTasks++;

    int clientSock = (intptr_t)arg;
    int bytesWritten;
    uint8_t prevRunIdle = 0;
    while (1) {
        if (prevRunIdle) vTaskDelay(1);
        xSemaphoreTake(logBufMutex, portMAX_DELAY);
        if (!logBufEmpty && (!backlogStarted || bufPtr != logBufWritePtr)) {
            prevRunIdle = 0;
            backlogStarted = 1;
            tcp_log_message_t* msg = &logBuf[bufPtr];
            bufPtr++;
            if (bufPtr >= CONFIG_TCP_LOG_BUFFER_SIZE) {
                bufPtr = 0;
            }
            xSemaphoreGive(logBufMutex);

            bytesWritten = send(clientSock, msg->text, msg->textLen, 0);
            if (bytesWritten < 0) {
                break;
            }
        } else {
            xSemaphoreGive(logBufMutex);
            prevRunIdle = 1;
        }
    }

    close(clientSock);
    numClientTasks--;

    vTaskDelete(NULL);
}

#endif
