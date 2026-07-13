#!/usr/bin/env python3
"""Cross-platform screenshot grabber for the Clawdmeter display.

Sends the firmware's `screenshot` serial command, reads the raw RGB565 frame
and writes a PNG. Pure stdlib + pyserial (no ffmpeg, no Pillow), so it runs
anywhere PlatformIO is installed — including Windows.

Usage:
    python screenshot.py <PORT> [output.png]

Examples:
    python screenshot.py COM6 usage.png           # Windows
    python screenshot.py /dev/ttyACM0 usage.png   # Linux
    python screenshot.py /dev/cu.usbmodem101       # macOS

On Windows, run it with the Python that ships with PlatformIO (it already has
pyserial), e.g.:
    & "$env:USERPROFILE\\.platformio\\penv\\Scripts\\python.exe" tools\\screenshot.py COM6 usage.png

Close any open `pio device monitor` first — the port can only be opened once.
"""
import sys
import struct
import zlib

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not found. Run with PlatformIO's Python (see the header of this file).")


def rgb565le_to_rgb888(data, w, h):
    out = bytearray(w * h * 3)
    for i in range(w * h):
        v = data[2 * i] | (data[2 * i + 1] << 8)   # little-endian
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        out[3 * i]     = (r << 3) | (r >> 2)
        out[3 * i + 1] = (g << 2) | (g >> 4)
        out[3 * i + 2] = (b << 3) | (b >> 2)
    return out


def write_png(path, w, h, rgb):
    def chunk(typ, data):
        return (struct.pack(">I", len(data)) + typ + data +
                struct.pack(">I", zlib.crc32(typ + data) & 0xFFFFFFFF))
    raw = bytearray()
    stride = w * 3
    for y in range(h):
        raw.append(0)                       # filter type 0 (None)
        raw += rgb[y * stride:(y + 1) * stride]
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
        f.write(chunk(b"IEND", b""))


def main():
    if len(sys.argv) < 2:
        sys.exit("Usage: python screenshot.py <PORT> [output.png]")
    port_path = sys.argv[1]
    out_path = sys.argv[2] if len(sys.argv) > 2 else "screenshot.png"

    # Baud is ignored by the ESP32-S3 native USB CDC, but pyserial needs a value.
    port = serial.Serial(port_path, 115200, timeout=15)
    port.reset_input_buffer()
    port.write(b"screenshot\n")
    port.flush()

    w = h = raw_size = 0
    while True:
        line = port.readline().decode("utf-8", errors="replace").strip()
        if line.startswith("SCREENSHOT_START"):
            _, sw, sh, sz = line.split()
            w, h, raw_size = int(sw), int(sh), int(sz)
            break
        if line == "SCREENSHOT_ERR":
            sys.exit("Device reported a screenshot error (LV_USE_SNAPSHOT off?).")
        if not line:
            sys.exit("Timed out waiting for the device. Is it on COM and not held by the monitor?")

    data = b""
    while len(data) < raw_size:
        chunk_ = port.read(min(4096, raw_size - len(data)))
        if not chunk_:
            sys.exit(f"Timeout: got {len(data)} of {raw_size} bytes.")
        data += chunk_
    port.close()

    write_png(out_path, w, h, rgb565le_to_rgb888(data, w, h))
    print(f"Saved {out_path} ({w}x{h})")


if __name__ == "__main__":
    main()
