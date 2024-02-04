#pragma once

#include <stdint.h>

typedef enum {
    MT_OVERWRITE,
    MT_KEEP_1,
    MT_KEEP_0
} buf_merge_t;

typedef enum {
    QUIRK_FLAG_COMBINING_FULL_STOP = (1 << 0),
} quirk_flag_t;


void buffer_8to1(uint8_t* buf8, uint8_t* buf1, uint16_t width, uint16_t height, buf_merge_t mergeType);
void buffer_utf8_to_iso88591(char* dst, char* src);
void buffer_iso88591_to_utf8(char* dst, char* src);
#if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
void buffer_textbuf_to_charbuf(uint8_t* display_text_buffer, uint8_t* display_char_buffer, uint16_t* display_quirk_flags_buffer, uint16_t textBufSize, uint16_t charBufSize);
#endif