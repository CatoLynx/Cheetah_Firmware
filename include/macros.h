#pragma once

/*
Explanation of the different buffer sizes

DISPLAY_BUF_SIZE[_*BPP]:      Number of pixels / characters on the display

DISPLAY_FRAMEBUF_SIZE[_*BPP]: Number of pixels / characters in the internal framebuffer[s].
                              This is the display data as sent to the display.

DISPLAY_CHARBUF_SIZE:         Number of characters in the internal character buffer.
                              This is the buffer that holds the display data before it is being converted
                              into the display-specific bytestream format.
                              If applicable, the quirk flag buffer is the same size and holds the quirk flags
                              for each character.

DISPLAY_TEXTBUF_SIZE:         Number of characters in the user-facing text buffer.
                              This is the buffer that holds the display data as entered by the user,
                              before handling things like line breaks.
*/

#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)

#define SET_MASK(buf, pos) (buf)[(pos)/8] |= (1 << ((pos)%8))
#define CLEAR_MASK(buf, pos) (buf)[(pos)/8] &= ~(1 << ((pos)%8))
#define GET_MASK(buf, pos) (!!((buf)[(pos)/8] & (1 << ((pos)%8))))

#if defined(CONFIG_DISPLAY_TYPE_CHARACTER)
#define STRCPY_TEXTBUF(dst, txt, len) strncpy(dst, txt, len);
#else
#define STRCPY_TEXTBUF(dst, txt, len)
#endif

#if defined(CONFIG_DISPLAY_TYPE_PIXEL) || defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    #define DISPLAY_HAS_SIZE
#endif

#if defined(DISPLAY_HAS_SIZE)
    #define DISPLAY_FRAME_WIDTH CONFIG_DISPLAY_FRAME_WIDTH
    #define DISPLAY_FRAME_HEIGHT CONFIG_DISPLAY_FRAME_HEIGHT
    #define DISPLAY_VIEWPORT_OFFSET_X CONFIG_DISPLAY_VIEWPORT_OFFSET_X
    #define DISPLAY_VIEWPORT_OFFSET_Y CONFIG_DISPLAY_VIEWPORT_OFFSET_Y
#endif

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
    #define DISPLAY_HAS_BRIGHTNESS_CONTROL 1
#else
    #define DISPLAY_HAS_BRIGHTNESS_CONTROL 0
#endif

#if defined(CONFIG_DISPLAY_TYPE_PIXEL)
    #define DISPLAY_TYPE "pixel"

    #define DISPLAY_BUF_SIZE_1BPP (CONFIG_DISPLAY_WIDTH * DIV_CEIL(CONFIG_DISPLAY_HEIGHT, 8))
    #define DISPLAY_BUF_SIZE_8BPP (CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT)
    #define DISPLAY_BUF_SIZE_24BPP (CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT * 3)

    #define DISPLAY_FRAMEBUF_SIZE_1BPP (CONFIG_DISPLAY_FRAME_WIDTH * DIV_CEIL(CONFIG_DISPLAY_FRAME_HEIGHT, 8))
    #define DISPLAY_FRAMEBUF_SIZE_8BPP (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT)
    #define DISPLAY_FRAMEBUF_SIZE_24BPP (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT * 3)

    #if defined(CONFIG_DISPLAY_FRAME_TYPE_1BPP)
        #define DISPLAY_FRAME_TYPE "1bpp"
        #define DISPLAY_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_1BPP
        #define DISPLAY_FRAME_HEIGHT_BYTES DIV_CEIL(CONFIG_DISPLAY_FRAME_HEIGHT, 8)
        #define BUFFER_VAL(buf, x, y) ((buf[(x * DISPLAY_FRAME_HEIGHT_BYTES) + (y / 8)] >> (y % 8)) & 1)
    #elif defined(CONFIG_DISPLAY_FRAME_TYPE_8BPP)
        #define DISPLAY_FRAME_TYPE "8bpp"
        #define DISPLAY_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_8BPP
        #define DISPLAY_FRAME_HEIGHT_BYTES CONFIG_DISPLAY_FRAME_HEIGHT
    #elif defined(CONFIG_DISPLAY_FRAME_TYPE_24BPP)
        #define DISPLAY_FRAME_TYPE "24bpp"
        #define DISPLAY_FRAMEBUF_SIZE DISPLAY_FRAMEBUF_SIZE_24BPP
        #define DISPLAY_FRAME_HEIGHT_BYTES (CONFIG_DISPLAY_FRAME_HEIGHT * 3)
    #endif
#elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    #define DISPLAY_TYPE "character"
    #define DISPLAY_FRAME_TYPE "char"
    #define DISPLAY_BUF_SIZE (CONFIG_DISPLAY_WIDTH * CONFIG_DISPLAY_HEIGHT)
    #define DISPLAY_CHARBUF_SIZE (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT)

    #if defined(CONFIG_DISPLAY_QUIRKS_COMBINING_FULL_STOP)
        // If the display has combining full stops, the text buffer needs to be
        // twice as large as the frame buffer, since every character could be succeeded by a full stop
        #define DISPLAY_TEXTBUF_SIZE (DISPLAY_CHARBUF_SIZE * 2)
    #else
        #define DISPLAY_TEXTBUF_SIZE DISPLAY_CHARBUF_SIZE
    #endif

    // TODO: Review this. Why is this different?
    #if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X)
        #define DISPLAY_FRAMEBUF_SIZE (DISPLAY_CHARBUF_SIZE * DIV_CEIL(CONFIG_DISPLAY_FRAMEBUF_BITS_PER_CHAR, 8))
    #else
        #define DISPLAY_FRAMEBUF_SIZE (DIV_CEIL(DISPLAY_CHARBUF_SIZE * CONFIG_DISPLAY_FRAMEBUF_BITS_PER_CHAR, 8))
    #endif
#elif defined(CONFIG_DISPLAY_TYPE_SELECTION)
    #define DISPLAY_TYPE "selection"
    #define DISPLAY_FRAME_TYPE "unit"
    #define DISPLAY_BUF_SIZE CONFIG_DISPLAY_UNIT_BUF_SIZE
    #define DISPLAY_FRAMEBUF_SIZE DISPLAY_BUF_SIZE
#endif

#if defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_LAWO_ALUMA)
    #define DISPLAY_DRIVER "flipdot_lawo_aluma"
#elif defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_BROSE)
    #define DISPLAY_DRIVER "flipdot_brose"
#elif defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_SAFLAP)
    #define DISPLAY_DRIVER "flipdot_saflap"
#elif defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER)
    #define DISPLAY_DRIVER "led_shift_register"
#elif defined(CONFIG_DISPLAY_DRIVER_LED_SHIFT_REGISTER_I2S)
    #define DISPLAY_DRIVER "led_shift_register_i2s"
#elif defined(CONFIG_DISPLAY_DRIVER_LED_AESYS_I2S)
    #define DISPLAY_DRIVER "led_aesys_i2s"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_SEG_LCD_SPI)
    #define DISPLAY_DRIVER "char_seg_lcd_spi"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_SPI)
    #define DISPLAY_DRIVER "char_16seg_led_spi"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_KRONE_9000)
    #define DISPLAY_DRIVER "char_krone_9000"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X)
    #define DISPLAY_DRIVER "char_16seg_led_ws281x"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_9000)
    #define DISPLAY_DRIVER "sel_krone_9000"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_8200_PST)
    #define DISPLAY_DRIVER "sel_krone_8200_pst"
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
    #if defined(CONFIG_DISPLAY_TYPE_PIXEL)
        #define SHADER_INCLUDE "shaders_pixel.h"
    #elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
        #define SHADER_INCLUDE "shaders_char.h"
    #endif
#endif