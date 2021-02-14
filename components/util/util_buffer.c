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