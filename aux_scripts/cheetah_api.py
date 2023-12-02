import base64
import requests


class Display:
    def __init__(self, host):
        self.host = host
        self.display_info = None
        self.device_info = None
        self.load_display_info()
        self.load_device_info()

    def get_actual_char_count(self, s):
        count = 0
        if "combining_full_stop" in self.display_info["quirks"]:
            for i in range(len(s)):
                if i == 0:
                    count += 1  # Always count first character
                elif s[i] == ".":
                    if s[i-1] == ".":
                        count += 1  # Only count full stop if preceded by another full stop
                else:
                    count += 1  # Always count non full stop characters
        else:
            count = len(s)
        return count

    def make_textbuf(self, text):
        textbuf = [0] * self.display_info["textbuf_size"]
        val_len = len(text)
        i = 0
        while i < val_len and i < self.display_info["textbuf_size"]:
            code = ord(val[i])
            textbuf[i] = code if code <= 255 else 0
            i += 1
        return framebuf

    def load_display_info(self):
        resp = requests.get(f"{self.host}/info/display.json")
        self.display_info = resp.json()

    def load_device_info(self):
        resp = requests.get(f"{self.host}/info/device.json")
        self.device_info = resp.json()

    def set_text(self, text):
        buf = bytearray(self.make_framebuf(text))
        buffer_b64 = base64.b64encode(buf).decode('ascii')
        requests.post(f"{self.host}/canvas/buffer.json", json={'buffer': buffer_b64})

    def set_brightness(self, brightness):
        assert brightness in range(0, 256)
        requests.post(f"{self.host}/canvas/brightness.json", json={'brightness': brightness})