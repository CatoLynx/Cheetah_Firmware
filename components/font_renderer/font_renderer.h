#pragma once

#include <stdint.h>
#include <stdio.h>


enum RLEFontGlyphFlag_e {
  RLEFontCharFlag_COMPRESSED       = 0x80,
};

struct RLEFontGlyph_s
{
    int32_t length; // length of data excluding padding
    uint8_t width; // width of the glyph in pixels
    uint8_t flags; // 0x80 == rleCompressed
    uint8_t glyphData[]; // padded to make length of RLEFontGlyph_s 32bit aligned
};

struct RLEFont_s
{
    int32_t height;
    //int32_t length;
    int32_t metadataOffset;
    int32_t glyphTableOffset;
    uint8_t data[];             // length Bytes
};

struct RLEFontMetadataEntry_s
{
    uint32_t nextOffset;
    uint32_t keyLength;
    uint32_t valueLength;
    uint8_t data[];
};

struct RLEFontGlyphTable_s
{
    uint32_t nextOffset;
    uint32_t start; // ordinal of first glyph
    uint32_t end;   // ordinal of last glyph
    uint32_t glyphDataOffset;
    uint32_t glyphOffsetsAndData[];
};

typedef struct RLEFontGlyph_s RLEFontGlyph_t;
typedef struct RLEFont_s RLEFont_t;
typedef struct RLEFontGlyphTable_s RLEFontGlyphTable_t;

RLEFontGlyph_t * rleFontGetPtr(uint32_t c, const RLEFontGlyphTable_t *font);
int32_t putLRECharAt(int32_t line, int32_t col, uint32_t c, int32_t h, const RLEFontGlyphTable_t *font, uint32_t *buffer);
int32_t putLREStringAt(int32_t line, int32_t col, uint8_t *s, const uint8_t *font, uint32_t *buffer);
uint32_t utf8DecodeChar(uint8_t **s);
