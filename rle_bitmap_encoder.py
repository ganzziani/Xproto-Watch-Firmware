"""RLE bitmap encoder for the Xproto-Watch firmware (bitmaps.c format).

Format (decoded by bitmap()/bitmap_safe() in Source/display.c):
  byte 0: width in pixels (max 128)
  byte 1: height in pixels (multiple of 8)
  data:   RLE over column-delta-filtered bytes.

The raster is column-major: width columns of height/8 bytes each, one byte
= 8 vertical pixels, MSB on top. Before RLE, every column except the first
is XORed with the previous column ("column delta") - vertically structured
art turns into long zero runs.

RLE format:  run     = <b> <b> <count>   (count = total bytes, 2..255)
             literal = <b>               (two equal literals never adjacent)

Usage:
  python rle_bitmap_encoder.py image.png [name]
     Requires Pillow. Non-white pixels are set. Prints a C array.
  Or import and call encode_raster(raw, width, height) with your own
  column-major bytes.
"""
import sys


def col_filter(raw, stride):
    out = bytearray(raw)
    for i in range(len(raw) - 1, stride - 1, -1):
        out[i] ^= raw[i - stride]
    return bytes(out)


def rle(stream):
    out = bytearray()
    i, n = 0, len(stream)
    while i < n:
        b = stream[i]
        run = 1
        while i + run < n and stream[i + run] == b and run < 255:
            run += 1
        if run >= 2:
            out += bytes((b, b, run))
            i += run
        else:
            out.append(b)
            i += 1
    return bytes(out)


def encode_raster(raw, width, height):
    """raw: column-major bytes (width columns x height/8 bytes). Returns the
    full array contents including the width/height header."""
    stride = height // 8
    assert height % 8 == 0 and len(raw) == width * stride
    return bytes((width, height)) + rle(col_filter(raw, stride))


def c_array(name, enc):
    lines = [f"const unsigned char {name}[] PROGMEM = {{ {enc[0]}, {enc[1]},"]
    data = enc[2:]
    for i in range(0, len(data), 8):
        chunk = data[i:i + 8]
        line = "    " + "".join(f"'\\x{b:02x}'," for b in chunk)
        if len(chunk) == 8:
            line += f" // 0x{i + 8:04x}"
        lines.append(line)
    lines.append("};")
    return "\n".join(lines)


def from_image(path):
    from PIL import Image
    img = Image.open(path).convert("L")
    w, h = img.size
    assert w <= 128 and h % 8 == 0, "width max 128, height multiple of 8"
    px = img.load()
    raw = bytearray()
    for x in range(w):
        for y8 in range(h // 8):
            b = 0
            for bit in range(8):
                if px[x, y8 * 8 + bit] < 128:      # dark pixel = set
                    b |= 0x80 >> bit
            raw.append(b)
    return bytes(raw), w, h


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    raw, w, h = from_image(sys.argv[1])
    name = sys.argv[2] if len(sys.argv) > 2 else "Bitmap"
    enc = encode_raster(raw, w, h)
    print(f"// {w}x{h}, {len(enc) - 2} bytes encoded ({len(raw)} raw)")
    print(c_array(name, enc))
