Import("env")

import shutil

print("Removing compiled Browser OTA component to update compile date and time")
try:
    shutil.rmtree(".pio/build/{}/components/browser_ota".format(env["PIOENV"]))
except FileNotFoundError:
    print("File not found")