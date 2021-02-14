#pragma once

#include <stdint.h>


#define ARTNET_HEADER_LENGTH 18
#define ARTNET_UNIVERSE_SIZE 512

typedef struct {
    uint16_t opcode;
    uint8_t sequence;
    uint16_t universe;
    uint16_t dataLength;
    uint8_t* data;
} artnetPacket_t;


void artnet_init(uint8_t* outBuf, uint8_t* tmpBuf, size_t outBufSize, size_t tmpBufSize);
void artnet_deinit(void);