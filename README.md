
# Cheetah Firmware
This is a firmware for ESP32 based display controllers. The goal is to have a modular system of inputs and display drivers that allows a certain level of mixing and matching for controlling all sorts of displays.

It is called "Cheetah" because that is the name of my custom ESP32 breakout board which has commonly used things for displays on board, like a fuse, reverse polarity protection, IBIS and RS485 interfaces, and a 24V power input. But it works perfectly fine with any ESP32 board - it doesn't require any special hardware.

## General information
This project is always under development and pretty much all features are developed based on spontaneous ideas and acute needs. Thus, some things are not 100% complete or compatible with all possible configurations.

## Supported displays
The firmware supports three classes of displays:
* Pixel-based displays, e.g. LCD, LED, flipdot matrix displays
* Character-based displays, e.g. mosaic LCD, 16-segment LED displays or alphanumeric split-flap units
* Character-based displays on top of pixel-based displays: i.e. a large character-based display that uses many LEDs to form distinct characters, but those LEDs can also be controlled as a pixel display, "modulated" by the character buffer.
* (Pixel-based displays on top of character-based displays: i.e. a large character-based display that can use its characters to display "pixel" images) - planned, but not yet supported
* Selection-based displays, which can only display a selection out of a fixed dataset, e.g. non-alphanumeric split-flap displays or rolling film displays

The following display drivers currently exist in this firmware:
| Name | Description |
|--|--|
| char_16seg_led_spi | Custom 16-segment LED boards with shift registers |
| char_16seg_led_ws281x | Custom 16-segment LED boards with WS281x RGB LEDs |
| char_krone_9000 | KRONE 9000 / 8200E split-flap system (alphanumeric) |
| char_segment_lcd_spi | AEG MIS GV07 mosaic LCD (incomplete, untested) |
| flipdot_aesco_saflap | Aesco SAFLAP flipdot boards using their one-wire control system |
| flipdot_brose | BROSE flipdot panels |
| flipdot_lawo_aluma | XY10 flipdot system with PCB coils (should be renamed at some point) |
| led | Generic shift-register based LED matrix with row addressing |
| led_aesys | aesys static drive LED panels |
| sel_krone_8200_pst | KRONE 8200 split-flap system, using the PST boards |
| sel_krone_9000 | KRONE 9000 / 8200E split-flap system (selection mode) |

## Supported input methods
The following input methods are supported:
| Name | Description |
|--|--|
| input_artnet | ArtNet (only for pixel displays) |
| input_browser_canvas | Interactive browser input |
| input_remote_poll | Periodic polling of framebuffer from configurable remote URL |
| input_telegram_bot | Telegram bot (only for character displays) |
| input_tpm2net | tpm2**​**.net protocol (only for pixel displays) |

## Other features
Some other noteworthy features include:
* Browser-based OTA interface (upload .bin file) with rollback (uploaded firmware needs to be verified, else previous version will be rolled back on reboot)
* Browser-based NVS key-value store
* Browser-based rudimentary SPIFFS file manager
* WireGuard implementation for easy access to the display from anywhere
* Experimental TCP-based log output

## Buffers and formats
Since this firmware supports many different types of displays, there is a need for multiple different kinds of buffers and formats.
One buffer is common to all of them: `display_output_buffer`, which holds the raw data that gets sent to the display in the format required by the display.
Depending on the project configuration, displays can use double buffering, in which case `prev_display_output_buffer` also exists.

Details about the buffers and the general operational workflow can be found in `docs/buffer_overview.pdf`.

### Pixel-based displays
Pixel-based displays use framebuffers. Depending on project configuration, the buffer can have one of three formats:

* 24bpp: RGB color, 8 bits per color, 24 bits per pixel
* 8bpp: Grayscale, 8 bits per pixel
* 1bpp: Only on or off, 1 bit per pixel

Pixel-based displays use the following buffers:

* `display_pixel_buffer`, which contains the bitmap data as vertical columns, starting at the top left, going down, from left to right
* `display_output_buffer`, as explained above

### Character-based displays
Character-based displays use the following buffers:

* `display_text_buffer`, which holds the input text as entered by the user
* `display_char_buffer`, which holds an intermediary representation of each character, e.g. a different character set
* `display_quirk_flags_buffer`, which holds additional data for each character, such as combining diacritical marks, in a display-specific format
* `display_output_buffer`, as explained above

### Character-on-pixel displays
Character-based displays on top of pixel-based displays basically modulate a pixel-based display with a character-based display and use the following buffers:

* `display_pixel_buffer`, which contains the bitmap data as vertical columns, starting at the top left, going down, from left to right
* `display_text_buffer`, which holds the input text as entered by the user
* `display_char_buffer`, which holds an intermediary representation of each character, e.g. a different character set
* `display_quirk_flags_buffer`, which holds additional data for each character, such as combining diacritical marks, in a display-specific format
* `display_output_buffer`, as explained above

### Pixel-on-character displays
Planned, but not yet implemented.

### Selection-based displays
Selection-based displays use the following buffers:

* `display_framebuf_mask`, which is used to know which unit addresses are in use
* `display_output_buffer`, which in this case consists of one byte per display unit (e.g. a split-flap module) and contains that unit's target position

The buffer mask is used because for selection displays, the firmware only defines the display driver and buffer size, but the actual configuration
of the layout and contents of the individual units is done via a JSON file uploaded to SPIFFS.
Thus, the buffer will usually be larger than required and the controller needs to know which parts of it to use.