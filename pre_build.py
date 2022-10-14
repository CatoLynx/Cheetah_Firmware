import shutil

print("Removing compiled Browser_OTA component to update compile date and time")
try:
    shutil.rmtree(".pio/build/esp-wrover-kit/components/Browser_OTA")
except FileNotFoundError:
    pass