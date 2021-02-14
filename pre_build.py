import shutil

try:
    shutil.rmtree(".pio/build/esp-wrover-kit/esp-idf/Browser_OTA")
except FileNotFoundError:
    pass