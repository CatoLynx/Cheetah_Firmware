import shutil

try:
    shutil.rmtree(".pio/build/esp-wrover-kit/components/Browser_OTA")
except FileNotFoundError:
    pass