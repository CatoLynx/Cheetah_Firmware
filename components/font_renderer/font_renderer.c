#include "font_renderer.h"


RLEFontGlyph_t * rleFontGetPtr(uint32_t c, const RLEFontGlyphTable_t *font)
{
    
    /*if(c < font->start || c > font->end)
    {
        if(font->nextOffset)
        {
            return rleFontGetPtr(c, ((uint8_t * )font) + font->nextOffset);
        } else
        {
            if ("?" > font->start && "?" < font->end)
            {
                c = '?';
            } else {
                return NULL;
            }
        }
        
    }
    
    int32_t index = (c - font->start);
    
    int32_t offset = font->glyphOffsetsAndData[index];
    RLEFontGlyph_t *fontPtr = (RLEFontGlyph_t *) (((uint8_t * )font) + font->glyphDataOffset + offset);
    return fontPtr;
    */
   return NULL;
}

int32_t putLRECharAt(int32_t line, int32_t col, uint32_t c, int32_t h, const RLEFontGlyphTable_t *font, uint32_t *buffer)
{
    //DEBUG("char is still %d\n\r", c);
    //delay_ms(1000);
    RLEFontGlyph_t *fontPtr = rleFontGetPtr(c, font);
    int32_t w = fontPtr->width;
    int8_t len = fontPtr->length;
    int8_t flags = fontPtr->flags;
    //DEBUG("Width: %d, len: %d\r\n", w, len);
    int32_t of = 0;
    //check if char is visible
    if(col + w >= 0 && col < 156)
    {
        int32_t line_of = line + h;
        if (flags & RLEFontCharFlag_COMPRESSED)
        {
            while(of < len)
            {
                int8_t rl = -fontPtr->glyphData[of++];

                printf("%ld:rl = %d -> ", of, rl);
                if(rl > 0)
                {
                    uint32_t d = fontPtr->glyphData[of++];
                    for(uint8_t i = 0; i < rl; i++)
                    {
                        printf("%ld %ld %02lX | ", col, line_of, d);
                        if(col >= 0 && col < 156)
                        {
                            buffer[col] |= d << line_of;
                        }
                        line_of -= 8;
                        if(line_of <= line)
                        {
                            line_of = line + h;
                            col++;
                        }
                    }
                } else
                {
                    for(int32_t i = 0; i < -rl; i++)
                    {
                        uint32_t d = fontPtr->glyphData[of++];
                        printf("%ld %ld %02lX | ", col, line_of, d);
                        if(col >= 0 && col < 156)
                        {
                            buffer[col] |= d << line_of;
                        }
                        line_of -= 8;
                        if(line_of <= line)
                        {
                            line_of = line + h;
                            col++;
                        }
                    }
                }
            //printf("\r\n\n\n");
            }
        } else {
            for(int32_t i = 0; i < len; i++)
                {
                    uint32_t d = fontPtr->glyphData[of++];
                    //printf("%d %d %02X | ", col, line_of, d);
                    if(col >= 0 && col < 156)
                    {
                        buffer[col] |= d << line_of;
                    }
                    line_of -= 8;
                    if(line_of <= line)
                    {
                        line_of = line + h;
                        col++;
                    }
                }
        }
    }
    return w;
}

int32_t putLREStringAt(int32_t line, int32_t col, uint8_t *s, const uint8_t *font, uint32_t *buffer)
{
    //DEBUG("Disp string at %d:%d '%s'\n\r", line, col, s);
    RLEFont_t * fontHead = (RLEFont_t *)font;
    //DEBUG("h:%d, glyph:%d\n\r", fontHead->height, fontHead->glyphTableOffset);
    const RLEFontGlyphTable_t * glyphTable = (RLEFontGlyphTable_t *)(&fontHead->data[fontHead->glyphTableOffset]);
    //DEBUG("font:%X, glyph:%X\n\r", fontHead, glyphTable);
    //delay_ms(1000);
    //DEBUG("next:%d\n\r", *glyphTable);
    //delay_ms(1000);
    while(*s)
    {
        uint32_t c = utf8DecodeChar(&s);
        //DEBUG("char is %d\n\r", c);
        //delay_ms(1000);
        col += putLRECharAt(line, col, c, fontHead->height, glyphTable, buffer);
    }
    return col;
}

/*
 *
 * Naive utf8 implementation
 *
 * Wont check for illegal chars
 */

uint32_t utf8DecodeChar(uint8_t **s)
{
    uint32_t c = *(*s)++;
    if(c & 0x80) // char is utf8
    {
        if(c & 0x40) // Start of 2
        {
            if(c & 0x20) // Start of 3
            {
                if(c & 0x10) // Start of 4
                {
                    c = c & 0x07;
                    c = (c << 6) | (*(*s)++ & 0x3F);
                } else
                {
                    c = c & 0x0F;
                }
                c = (c << 6) | (*(*s)++ & 0x3F);
            } else
            {
                c = c & 0x1F;
            }
            c = (c << 6) | (*(*s)++ & 0x3F);
        } else
        {
            printf("UTF8 Decoding error! c = %08lX", c);
        }
    }
    return c;
}
