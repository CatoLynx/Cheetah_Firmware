import ctypes
import datetime
import time
import tkinter as tk
from tkinter import Canvas
from PIL import Image, ImageTk

# Load the DLL
dll = ctypes.CDLL('./bitmap_generators.dll')

# Define the function prototypes
dll.solid_single.argtypes = [ctypes.c_int64, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
dll.rainbow_t.argtypes = [ctypes.c_int64, ctypes.c_uint16]
dll.rainbow_gradient.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
dll.hard_gradient_3.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
dll.soft_gradient_3.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
dll.on_off_100_frames.argtypes = [ctypes.c_int64]
dll.matrix.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]
dll.set_pixel_buffer.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
dll.plasma.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
dll.plasma_2.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8, ctypes.c_uint8]

# Define the frame size and upscaling factor
frame_width = 74
frame_height = 101
upscaling_factor = 3

# Define the pixel buffer and size
pixel_buffer_size = frame_width * frame_height * 3  # 3 bytes per pixel (RGB)
pixel_buffer = (ctypes.c_uint8 * pixel_buffer_size)()

# Set the buffer and size in the DLL
dll.set_pixel_buffer(pixel_buffer, pixel_buffer_size, frame_width, frame_height)

# Initialize the tkinter window
root = tk.Tk()
canvas = Canvas(root, width=frame_width * upscaling_factor, height=frame_height * upscaling_factor)
canvas.pack()

def update_frame(t):
    # Call the generator function
    #dll.solid_single(t, 1, 0, 0.5)
    #dll.rainbow_t(t, 100)
    #dll.rainbow_gradient(t, 64, 0, 100, 255, 255)
    #dll.hard_gradient_3(t, 25, 97, 100, 255, 33, 140, 255, 216, 0, 33, 177, 255)
    dll.soft_gradient_3(t, 25, 97, 100, 255, 0, 128, 0, 255, 0, 0, 255, 255)
    #dll.on_off_100_frames(t)
    #dll.matrix(t, 10, 0, 255, 0, 127, 255, 127)
    #dll.plasma(t, 7, 20, 255, 255)
    #dll.plasma_2(t, 7, 20, 255, 255, 255, 0, 0, 0)

    # Create an image from the pixel buffer
    img = Image.new('RGB', (frame_width, frame_height), 'black')
    pixels = img.load()
    for i in range(0, pixel_buffer_size - 2, 3):
        x = i // (frame_height * 3)
        y = i % (frame_height * 3) // 3
        try:
            pixels[x,y] = (pixel_buffer[i], pixel_buffer[i+1], pixel_buffer[i+2])
        except:
            print((i, pixel_buffer_size, x, y))
    img = img.resize((frame_width * upscaling_factor, frame_height * upscaling_factor), Image.NEAREST)
    img_tk = ImageTk.PhotoImage(img)

    # Update the canvas with the new image
    canvas.create_image(0, 0, anchor=tk.NW, image=img_tk)
    canvas.image = img_tk  # Prevent garbage collection
    root.update()

# Run the animation loop
t = 0
while True:
    now = datetime.datetime.now()
    t = int(time.mktime(now.timetuple()) * 1000000 + now.microsecond)
    update_frame(t)

root.mainloop()
