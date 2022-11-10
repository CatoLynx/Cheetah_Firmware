#include <stdint.h>
#include <stdlib.h>

typedef struct {
    char* text;
    int textLen;
} tcp_log_message_t;


void tcp_log_init();
void tcp_log_start();
int tcp_log_vprintf(const char* format, va_list args);
void tcp_log_addToBuffer(tcp_log_message_t* msg);
void tcp_log_server_task(void* arg);
void tcp_log_client_handler_task(void* arg);