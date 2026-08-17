#!/usr/bin/env python3
"""Capture /dev/fb0 (RGB565) as a PNG. Usage: capture_fb.py <input.raw> <output.png> [w] [h]"""
import struct
import sys
import zlib


def paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def rgb565_to_png(raw, width, height):
    img = bytearray(width * height * 3)
    for i in range(width * height):
        p = struct.unpack_from("<H", raw, i * 2)[0]
        r = (p >> 11) & 0x1F
        r = (r << 3) | (r >> 2)
        g = (p >> 5) & 0x3F
        g = (g << 2) | (g >> 4)
        b = p & 0x1F
        b = (b << 3) | (b >> 2)
        img[i * 3] = r
        img[i * 3 + 1] = g
        img[i * 3 + 2] = b

    stride = width * 3
    raw_data = b""
    prev = bytes(stride)
    for y in range(height):
        row = bytes(img[y * stride:(y + 1) * stride])
        filt = bytearray([0])
        for x in range(width):
            idx = x * 3
            for ch in range(3):
                a = row[idx + ch - 3] if x > 0 else 0
                b = prev[idx + ch]
                c = prev[idx + ch - 3] if x > 0 else 0
                filt.append((row[idx + ch] - paeth(a, b, c)) & 0xFF)
        raw_data += filt
        prev = row

    def chunk(ctype, data):
        c = ctype + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", zlib.compress(raw_data)) + chunk(b"IEND", b""))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 1
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 320
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 170
    with open(sys.argv[1], "rb") as f:
        raw = f.read(width * height * 2)
    with open(sys.argv[2], "wb") as f:
        f.write(rgb565_to_png(raw, width, height))
    return 0


if __name__ == "__main__":
    sys.exit(main())
