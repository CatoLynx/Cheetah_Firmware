Import("env")

import os
import subprocess

print("Removing compiled main/httpd component to update compile date and time")
try:
    os.remove(".pio/build/{}/src/httpd.c.o".format(env["PIOENV"]))
except FileNotFoundError:
    print("File not found")

print("Writing git describe output to file")
try:
    version = subprocess.check_output("git describe --always --dirty=-d", shell=True).strip().decode('utf-8').upper()
    print("Version:", version)
    with open("include/git_version.h", 'w') as f:
        f.write("#pragma once\n\n#define GIT_VERSION \"{}\"\n".format(version))
except:
    print("Failed to get output")