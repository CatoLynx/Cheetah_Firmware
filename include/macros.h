/*
Explanation of the different buffer sizes

DISPLAY_PIX_BUF_SIZE[_*BPP]:        Number of pixels in the internal pixel buffer.
                                    Can be more than fits on the viewport in order to allow scrolling.

DISPLAY_OUT_BUF_SIZE[_*BPP]:        Number of pixels / characters in the output data buffer[s].
                                    This is the display data as sent to the display.

DISPLAY_CHAR_BUF_SIZE:              Number of characters in the internal character buffer.
                                    This is the buffer that holds the display text data before it is being converted
                                    into the display-specific bytestream format.
                                    If applicable, the quirk flag buffer is the same size and holds the quirk flags
                                    for each character.

DISPLAY_TEXT_BUF_SIZE:              Number of characters in the user-facing text buffer.
                                    This is the buffer that holds the display data as entered by the user,
                                    before handling things like line breaks.

DISPLAY_UNIT_BUF_SIZE:              Number of position slots for selection display units.
*/

#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)

#define SET_MASK(buf, pos) (buf)[(pos)/8] |= (1 << ((pos)%8))
#define CLEAR_MASK(buf, pos) (buf)[(pos)/8] &= ~(1 << ((pos)%8))
#define GET_MASK(buf, pos) (!!((buf)[(pos)/8] & (1 << ((pos)%8))))

#if defined(CONFIG_DISPLAY_TYPE_PIXEL)
    #define DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL
#elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    #define DISPLAY_OUTPUT_BUFFER_BASED_ON_CHAR
#elif defined(CONFIG_DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL)
    #define DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL
#elif defined(CONFIG_DISPLAY_OUTPUT_BUFFER_BASED_ON_CHAR)
    #define DISPLAY_OUTPUT_BUFFER_BASED_ON_CHAR
#endif

#if defined(CONFIG_DISPLAY_TYPE_PIXEL) || defined(CONFIG_DISPLAY_TYPE_CHAR_ON_PIXEL) || defined(CONFIG_DISPLAY_TYPE_PIXEL_ON_CHAR)
    #define DISPLAY_HAS_PIXEL_BUFFER
#endif

#if defined(CONFIG_DISPLAY_TYPE_CHARACTER) || defined(CONFIG_DISPLAY_TYPE_CHAR_ON_PIXEL) || defined(CONFIG_DISPLAY_TYPE_PIXEL_ON_CHAR)
    #define DISPLAY_HAS_TEXT_BUFFER
#endif

#if defined(DISPLAY_HAS_TEXT_BUFFER)
#define STRCPY_TEXTBUF(dst, txt, len) strncpy(dst, txt, len);
#else
#define STRCPY_TEXTBUF(dst, txt, len)
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    #define DISPLAY_VIEWPORT_WIDTH_PIXEL CONFIG_DISPLAY_VIEWPORT_WIDTH_PIXEL
    #define DISPLAY_VIEWPORT_HEIGHT_PIXEL CONFIG_DISPLAY_VIEWPORT_HEIGHT_PIXEL
    #define DISPLAY_FRAME_WIDTH_PIXEL CONFIG_DISPLAY_FRAME_WIDTH_PIXEL
    #define DISPLAY_FRAME_HEIGHT_PIXEL CONFIG_DISPLAY_FRAME_HEIGHT_PIXEL
    #define DISPLAY_VIEWPORT_OFFSET_X_PIXEL CONFIG_DISPLAY_VIEWPORT_OFFSET_X_PIXEL
    #define DISPLAY_VIEWPORT_OFFSET_Y_PIXEL CONFIG_DISPLAY_VIEWPORT_OFFSET_Y_PIXEL
    #define DISPLAY_OUTPUT_WIDTH CONFIG_DISPLAY_OUTPUT_WIDTH
    #define DISPLAY_OUTPUT_HEIGHT CONFIG_DISPLAY_OUTPUT_HEIGHT
#endif

#if defined(DISPLAY_HAS_TEXT_BUFFER)
    #define DISPLAY_VIEWPORT_WIDTH_CHAR CONFIG_DISPLAY_VIEWPORT_WIDTH_CHAR
    #define DISPLAY_VIEWPORT_HEIGHT_CHAR CONFIG_DISPLAY_VIEWPORT_HEIGHT_CHAR
    #define DISPLAY_FRAME_WIDTH_CHAR CONFIG_DISPLAY_FRAME_WIDTH_CHAR
    #define DISPLAY_FRAME_HEIGHT_CHAR CONFIG_DISPLAY_FRAME_HEIGHT_CHAR
    #define DISPLAY_VIEWPORT_OFFSET_X_CHAR CONFIG_DISPLAY_VIEWPORT_OFFSET_X_CHAR
    #define DISPLAY_VIEWPORT_OFFSET_Y_CHAR CONFIG_DISPLAY_VIEWPORT_OFFSET_Y_CHAR
    #define DISPLAY_OUTPUT_WIDTH CONFIG_DISPLAY_OUTPUT_WIDTH
    #define DISPLAY_OUTPUT_HEIGHT CONFIG_DISPLAY_OUTPUT_HEIGHT
#endif

#if defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL)
    #define DISPLAY_OUTPUT_BUFFER_BASED_ON "pixel"
#elif defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_CHAR)
    #define DISPLAY_OUTPUT_BUFFER_BASED_ON "char"
#endif

#if defined(CONFIG_DISPLAY_HAS_BRIGHTNESS_CONTROL)
    #define DISPLAY_HAS_BRIGHTNESS_CONTROL 1
#else
    #define DISPLAY_HAS_BRIGHTNESS_CONTROL 0
#endif

#if defined(CONFIG_DISPLAY_TYPE_PIXEL)
    #define DISPLAY_TYPE "pixel"
#elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    #define DISPLAY_TYPE "character"
#elif defined(CONFIG_DISPLAY_TYPE_CHAR_ON_PIXEL)
    #define DISPLAY_TYPE "char_on_pixel"
#elif defined(CONFIG_DISPLAY_TYPE_PIXEL_ON_CHAR)
    #define DISPLAY_TYPE "pixel_on_char"
#elif defined(CONFIG_DISPLAY_TYPE_SELECTION)
    #define DISPLAY_TYPE "selection"
    #define DISPLAY_UNIT_BUF_SIZE CONFIG_DISPLAY_UNIT_BUF_SIZE
    #define DISPLAY_OUT_BUF_SIZE DISPLAY_UNIT_BUF_SIZE
#endif

#if defined(DISPLAY_HAS_PIXEL_BUFFER)
    #define DISPLAY_PIX_BUF_SIZE_1BPP (DISPLAY_FRAME_WIDTH_PIXEL * DIV_CEIL(DISPLAY_FRAME_HEIGHT_PIXEL, 8))
    #define DISPLAY_PIX_BUF_SIZE_8BPP (DISPLAY_FRAME_WIDTH_PIXEL * DISPLAY_FRAME_HEIGHT_PIXEL)
    #define DISPLAY_PIX_BUF_SIZE_24BPP (DISPLAY_FRAME_WIDTH_PIXEL * DISPLAY_FRAME_HEIGHT_PIXEL * 3)

    #if defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL)
        #define DISPLAY_OUT_BUF_SIZE_1BPP (CONFIG_DISPLAY_OUTPUT_WIDTH * DIV_CEIL(CONFIG_DISPLAY_OUTPUT_HEIGHT, 8))
        #define DISPLAY_OUT_BUF_SIZE_8BPP (CONFIG_DISPLAY_OUTPUT_WIDTH * CONFIG_DISPLAY_OUTPUT_HEIGHT)
        #define DISPLAY_OUT_BUF_SIZE_24BPP (CONFIG_DISPLAY_OUTPUT_WIDTH * CONFIG_DISPLAY_OUTPUT_HEIGHT * 3)
    #endif

    #if defined(CONFIG_DISPLAY_PIX_BUF_TYPE_1BPP)
        #define DISPLAY_PIX_BUF_TYPE "1bpp"
        #define DISPLAY_PIX_BUF_SIZE DISPLAY_PIX_BUF_SIZE_1BPP
        #if defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL)
            #define DISPLAY_OUT_BUF_SIZE DISPLAY_OUT_BUF_SIZE_1BPP
        #endif
        #define DISPLAY_FRAME_HEIGHT_PIXEL_BYTES DIV_CEIL(DISPLAY_FRAME_HEIGHT_PIXEL, 8)
        #define PIX_BUF_VAL(buf, x, y) ((buf[(x * DISPLAY_FRAME_HEIGHT_PIXEL_BYTES) + (y / 8)] >> (y % 8)) & 1)
    #elif defined(CONFIG_DISPLAY_PIX_BUF_TYPE_8BPP)
        #define DISPLAY_PIX_BUF_TYPE "8bpp"
        #define DISPLAY_PIX_BUF_SIZE DISPLAY_PIX_BUF_SIZE_8BPP
        #if defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL)
            #define DISPLAY_OUT_BUF_SIZE DISPLAY_OUT_BUF_SIZE_8BPP
        #endif
        #define DISPLAY_FRAME_HEIGHT_PIXEL_BYTES DISPLAY_FRAME_HEIGHT_PIXEL
    #elif defined(CONFIG_DISPLAY_PIX_BUF_TYPE_24BPP)
        #define DISPLAY_PIX_BUF_TYPE "24bpp"
        #define DISPLAY_PIX_BUF_SIZE DISPLAY_PIX_BUF_SIZE_24BPP
        #if defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_PIXEL)
            #define DISPLAY_OUT_BUF_SIZE DISPLAY_OUT_BUF_SIZE_24BPP
        #endif
        #define DISPLAY_FRAME_HEIGHT_PIXEL_BYTES (DISPLAY_FRAME_HEIGHT_PIXEL * 3)
    #endif
#endif

#if defined(DISPLAY_HAS_TEXT_BUFFER)
    #define DISPLAY_CHAR_BUF_SIZE (DISPLAY_FRAME_WIDTH_CHAR * DISPLAY_FRAME_HEIGHT_CHAR)

    #if defined(CONFIG_DISPLAY_QUIRKS_COMBINING_FULL_STOP)
        // If the display has combining full stops, the text buffer needs to be
        // twice as large as the char buffer, since every character could be succeeded by a full stop
        #define DISPLAY_TEXT_BUF_SIZE (DISPLAY_CHAR_BUF_SIZE * 2)
    #else
        #define DISPLAY_TEXT_BUF_SIZE DISPLAY_CHAR_BUF_SIZE
    #endif

    #if defined(DISPLAY_OUTPUT_BUFFER_BASED_ON_CHAR)
        // TODO: Review this. Why is this different?
        #if defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X) || defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X_HYBRID)
            #define DISPLAY_OUT_BUF_SIZE (DISPLAY_CHAR_BUF_SIZE * DIV_CEIL(CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR, 8))
        #else
            #define DISPLAY_OUT_BUF_SIZE (DIV_CEIL(DISPLAY_CHAR_BUF_SIZE * CONFIG_DISPLAY_OUT_BUF_BITS_PER_CHAR, 8))
        #endif
    #endif
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
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_IBIS)
    #define DISPLAY_DRIVER "char_ibis"
#elif defined(CONFIG_DISPLAY_DRIVER_CHAR_16SEG_LED_WS281X_HYBRID)
    #define DISPLAY_DRIVER "char_16seg_led_ws281x_hybrid"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_9000)
    #define DISPLAY_DRIVER "sel_krone_9000"
#elif defined(CONFIG_DISPLAY_DRIVER_SEL_KRONE_8200_PST)
    #define DISPLAY_DRIVER "sel_krone_8200_pst"
#endif

#if defined(CONFIG_DISPLAY_HAS_SHADERS)
    #define SHADERS_INCLUDE CONFIG_DISPLAY_SHADERS_INCLUDE
#endif

#if defined(CONFIG_DISPLAY_HAS_TRANSITIONS)
    #define TRANSITIONS_INCLUDE CONFIG_DISPLAY_TRANSITIONS_INCLUDE
#endif

#if defined(CONFIG_DISPLAY_HAS_EFFECTS)
    #define EFFECTS_INCLUDE CONFIG_DISPLAY_EFFECTS_INCLUDE
#endif