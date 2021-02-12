#define DIV_CEIL(x, y) ((x % y) ? x / y + 1 : x / y)

#if defined(CONFIG_DISPLAY_TYPE_PIXEL)
    #define DISPLAY_TYPE "pixel"
    #define DISPLAY_FRAME_WIDTH CONFIG_DISPLAY_FRAME_WIDTH
    #define DISPLAY_FRAME_HEIGHT CONFIG_DISPLAY_FRAME_HEIGHT
    #define DISPLAY_VIEWPORT_OFFSET_X CONFIG_DISPLAY_VIEWPORT_OFFSET_X
    #define DISPLAY_VIEWPORT_OFFSET_Y CONFIG_DISPLAY_VIEWPORT_OFFSET_Y

    #if defined(CONFIG_DISPLAY_FRAME_TYPE_1BPP)
        #define DISPLAY_FRAME_TYPE "1bpp"
        #define DISPLAY_FRAMEBUF_SIZE DIV_CEIL(CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT, 8)
    #elif defined(CONFIG_DISPLAY_FRAME_TYPE_8BPP)
        #define DISPLAY_FRAME_TYPE "8bpp"
        #define DISPLAY_FRAMEBUF_SIZE (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT)
    #elif defined(CONFIG_DISPLAY_FRAME_TYPE_24BPP)
        #define DISPLAY_FRAME_TYPE "24bpp"
        #define DISPLAY_FRAMEBUF_SIZE (CONFIG_DISPLAY_FRAME_WIDTH * CONFIG_DISPLAY_FRAME_HEIGHT * 3)
    #endif
#elif defined(CONFIG_DISPLAY_TYPE_CHARACTER)
    #define DISPLAY_TYPE "character"
    #define DISPLAY_FRAME_WIDTH 0
    #define DISPLAY_FRAME_HEIGHT 0
    #define DISPLAY_VIEWPORT_OFFSET_X 0
    #define DISPLAY_VIEWPORT_OFFSET_Y 0
    #define DISPLAY_FRAME_TYPE ""
    #define DISPLAY_FRAMEBUF_SIZE 0
#endif

#if defined(CONFIG_DISPLAY_DRIVER_FLIPDOT_LAWO_ALUMA)
    #define DISPLAY_DRIVER "flipdot_lawo_aluma"
#endif