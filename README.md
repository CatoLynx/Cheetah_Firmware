
# Cheetah Firmware
This is a firmware for ESP32 based display controllers. The goal is to have a modular system of inputs and display drivers that allows a certain level of mixing and matching for controlling all sorts of displays.

It is called "Cheetah" because that is the name of my custom ESP32 breakout board which has commonly used things for displays on board, like a fuse, reverse polarity protection, IBIS and RS485 interfaces, and a 24V power input. But it works perfectly fine with any ESP32 board - it doesn't require any special hardware.

## General information
This project is always under development and pretty much all features are developed based on spontaneous ideas and acute needs. Thus, some things are not 100% complete or compatible with all possible configurations.

## Supported displays
The firmware supports three classes of displays:
* Pixel-based displays, e.g. LCD, LED, flipdot matrix displays
* Character-based displays, e.g. mosaic LCD, 16-segment LED displays or alphanumeric split-flap units
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
