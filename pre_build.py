Import("env")

import os

print("Removing compiled main/httpd component to update compile date and time")
try:
    os.remove(".pio/build/{}/src/httpd.o".format(env["PIOENV"]))
except FileNotFoundError:
    print("File not found")