#include "util_buffer.h"
#include "util_generic.h"


void buffer_8to1(uint8_t* buf8, uint8_t* buf1, uint16_t width, uint16_t height, buf_merge_t mergeType) {
    // Convert an 8bpp buffer into a 1bpp buffer
    uint32_t buf1ByteIdx;
    uint8_t buf1BitIdx;
    uint32_t buf8Idx;
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            buf1ByteIdx = x * DIV_CEIL(height, 8) + y / 8;
            buf1BitIdx = y % 8;
            buf8Idx = x * height + y;

            // Data bit
            if (buf8[buf8Idx] > 127) {
                if (mergeType == MT_OVERWRITE || mergeType == MT_KEEP_1) buf1[buf1ByteIdx] |= (1 << buf1BitIdx);
            } else {
                if (mergeType == MT_OVERWRITE || mergeType == MT_KEEP_0) buf1[buf1ByteIdx] &= ~(1 << buf1BitIdx);
            }
        }
    }
}

void buffer_utf8_to_iso88591(char* dst, char* src) {
    if (src == NULL || dst == NULL) return;

    uint32_t codepoint = 0;
    while (*src) {
        unsigned char ch = (unsigned char)*src;
        if (ch <= 0x7f) codepoint = ch;
        else if (ch <= 0xbf) codepoint = (codepoint << 6) | (ch & 0x3f);
        else if (ch <= 0xdf) codepoint = ch & 0x1f;
        else if (ch <= 0xef) codepoint = ch & 0x0f;
        else codepoint = ch & 0x07;
        src++;
        if (((*src & 0xc0) != 0x80) && (codepoint <= 0x10ffff)) {
            if (codepoint <= 0xFF) {
                *dst = (char)codepoint;
                dst++;
            } else {
                // Character not in range [0 ... 255], skip
            }
        }
    }
}

void buffer_iso88591_to_utf8(char* dst, char* src) {
    if (src == NULL || dst == NULL) return;
    
    while (*src) {
        if (*src <= 0x7f) {
            *dst++ = *src++;
        } else {
            *dst++ = 0xc2 + (*src > 0xbf);
            *dst++ = (*src++ & 0x3f) + 0x80;
        }
    }
}