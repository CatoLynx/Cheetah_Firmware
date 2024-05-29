import ctypes
import time
import tkinter as tk
from tkinter import Canvas
from PIL import Image, ImageTk

# Load the DLL
dll = ctypes.CDLL('./bitmap_generators.dll')

# Define the function prototypes
dll.solid_single.argtypes = [ctypes.c_int64, ctypes.c_double, ctypes.c_double, ctypes.c_double]
dll.rainbow_t.argtypes = [ctypes.c_int64, ctypes.c_uint16]
dll.rainbow_gradient.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint8, ctypes.c_uint8]
dll.hard_gradient_3.argtypes = [ctypes.c_int64, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_uint16, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double]
dll.set_pixel_buffer.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]

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
    #dll.rainbow_gradient(t, 100, 45, 100, 255, 255)
    dll.hard_gradient_3(t, 25, 97, 100, 1.0, 0.129, 0.549, 1.0, 0.847, 0.0, 0.129, 0.694, 1.0)

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
    update_frame(t)
    t += 10000  # Increment time (example: 10 milliseconds)
    time.sleep(0.01)  # Delay to control frame rate

root.mainloop()
