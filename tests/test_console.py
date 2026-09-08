"""Exercise the actual freestanding renderer against guarded host memory.

Run from the repository root: python3 tests/test_console.py
Only Clang/LLD and Python's standard library are required.
"""
import ctypes as C
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'build' / 'tests'
OUT.mkdir(parents=True, exist_ok=True)
lib_path = OUT / ('console.dll' if os.name == 'nt' else 'console.so')
command = ['clang', '-ffreestanding', '-fno-stack-protector', '-O2',
           '-Wall', '-Wextra', '-Werror', '-shared', '-nostdlib',
           str(ROOT / 'kernel/console.c'), str(ROOT / 'kernel/framebuffer.c'), '-o', str(lib_path)]
if os.name == 'nt':
    command += ['-fuse-ld=lld', '-Wl,/noentry', '-Wl,/export:draw_char',
                '-Wl,/export:draw_string', '-Wl,/export:console_init',
                '-Wl,/export:console_clear', '-Wl,/export:console_write',
                '-Wl,/export:fb_fill_rect', '-Wl,/export:fb_fill',
                '-Wl,/export:fb_outline', '-Wl,/export:fb_copy_rect',
                '-Wl,/export:fb_color']
else:
    command += ['-fPIC']
subprocess.run(command, check=True)
lib = C.CDLL(str(lib_path))


class Handoff(C.Structure):
    _fields_ = [('magic', C.c_uint64), ('framebuffer', C.c_uint64),
                ('fb_width', C.c_uint32), ('fb_height', C.c_uint32),
                ('fb_pitch', C.c_uint32), ('fb_bpp', C.c_uint32),
                ('fb_format', C.c_uint32)]


lib.draw_char.argtypes = [C.POINTER(Handoff), C.c_char, C.c_int, C.c_int, C.c_uint32]
lib.draw_string.argtypes = [C.POINTER(Handoff), C.c_char_p, C.c_int, C.c_int, C.c_uint32]
BG, FG, GUARD = 0x102030, 0xABCDEF, 0xBADCAFE


def surface(width=24, height=16, stride=29):
    data = (C.c_uint32 * (stride * height + 2))()
    for i in range(len(data)):
        data[i] = GUARD
    for y in range(height):
        for x in range(width):
            data[1 + y * stride + x] = BG
    return data, Handoff(0, C.addressof(data) + 4, width, height, stride * 4, 32, 0)


def pixels(data, ho):
    stride = ho.fb_pitch // 4
    assert data[0] == data[-1] == GUARD
    for y in range(ho.fb_height):
        assert all(data[1 + y * stride + x] == GUARD
                   for x in range(ho.fb_width, stride)), 'pitch padding overwritten'
    return {(x, y) for y in range(ho.fb_height) for x in range(ho.fb_width)
            if data[1 + y * stride + x] == FG}


data, ho = surface()
lib.draw_char(C.byref(ho), b'A', 0, 0, FG)
shape = pixels(data, ho)
assert 0 < len(shape) < 64, 'glyph must not be an empty or solid square'
assert (3, 0) in shape and (0, 0) not in shape

# Clipping at every edge is a translated intersection of the full glyph.
for x, y in [(-3, -2), (21, 13), (-8, 0), (24, 0), (0, 16),
             (2**31 - 1, 0), (-2**31, 0)]:
    data, ho = surface()
    lib.draw_char(C.byref(ho), b'A', x, y, FG)
    assert pixels(data, ho) == {(a+x, b+y) for a, b in shape
                               if 0 <= a+x < 24 and 0 <= b+y < 16}

data, ho = surface()
lib.draw_string(C.byref(ho), b'A A', 0, 0, FG)
assert pixels(data, ho) == shape | {(x+16, y) for x, y in shape}
lib.draw_string(C.byref(ho), None, 0, 0, FG)
lib.draw_char(None, b'A', 0, 0, FG)

for ch in range(33, 127):
    data, ho = surface()
    lib.draw_char(C.byref(ho), bytes([ch]), 0, 0, FG)
    assert pixels(data, ho), f'empty ASCII glyph {ch}'

fallback = None
for ch in [63, 0, 127, 255]:
    data, ho = surface()
    lib.draw_char(C.byref(ho), bytes([ch]), 0, 0, FG)
    if fallback is None:
        fallback = pixels(data, ho)
    assert pixels(data, ho) == fallback

for field, value in [('framebuffer', 0), ('fb_bpp', 24), ('fb_pitch', 4),
                     ('fb_pitch', 97), ('fb_width', 0), ('fb_height', 0)]:
    data, ho = surface()
    before = list(data)
    setattr(ho, field, value)
    lib.draw_char(C.byref(ho), b'A', 0, 0, FG)
    assert list(data) == before, 'invalid framebuffer must be ignored'
print('PASS: ASCII glyphs, transparency, fallback, clipping, stride and guards')

lib.console_init.argtypes = [C.POINTER(Handoff)]
lib.console_write.argtypes = [C.c_char_p]
lib.console_clear.argtypes = []


def console_surface(fmt=0, width=24, height=65, stride=29):
    data, ho = surface(width, height, stride)
    ho.fb_format = fmt
    lib.console_init(C.byref(ho))
    return data, ho


def cell(data, ho, x, y):
    stride = ho.fb_pitch // 4
    return [data[1+(40+y*8+dy)*stride+x*8+dx] for dy in range(8) for dx in range(8)]


for fmt, bg in [(0, 0x2D0F0F), (1, 0x0F0F2D)]:
    data, ho = console_surface(fmt)
    before = list(data)
    assert cell(data, ho, 1, 0) == [bg]*64
    assert cell(data, ho, 0, 0)[56:] == [0xFFFFFF]*8
    lib.console_write(b'A')
    first = cell(data, ho, 0, 0)
    lib.console_write(b'B\b \b')
    assert cell(data, ho, 0, 0) == first, 'backspace damaged preceding glyph'
    assert cell(data, ho, 1, 0)[:56] == [bg]*56, 'backspace sequence did not erase'
    lib.console_write(b'\rC')
    assert cell(data, ho, 0, 0) != first, 'carriage return did not overwrite'
    lib.console_clear()
    lib.console_write(b'A\nB\nC')
    second, third = cell(data, ho, 0, 1), cell(data, ho, 0, 2)
    lib.console_write(b'\n')
    assert cell(data, ho, 0, 0) == second, 'scroll did not move row 1 up'
    assert cell(data, ho, 0, 1) == third, 'scroll did not move row 2 up'
    assert cell(data, ho, 1, 2) == [bg]*64, 'scroll bottom row not cleared'
    lib.console_clear()
    lib.console_write(b'ABC')
    assert cell(data, ho, 0, 1)[56:] == [0xFFFFFF]*8, 'automatic wrapping failed'
    lib.console_write(b'\b \b')
    assert cell(data, ho, 2, 0)[:56] == [bg]*56, 'backspace across wrap failed'
    lib.console_clear()
    lib.console_write(b'\b\t')
    assert cell(data, ho, 1, 1)[56:] == [0xFFFFFF]*8, 'tab stop/wrapping failed'
    # Repeated scrolling must never touch the title, partial bottom row or guards.
    lib.console_write(b'line\n'*100)
    assert list(data)[:1+40*29] == before[:1+40*29]
    assert list(data)[1+64*29:] == before[1+64*29:]
    pixels(data, ho)

# Smallest viewport: scrolling has no source row. Reject zero-row viewports.
data, ho = console_surface(width=8, height=48)
lib.console_write(b'A\nB\n'*10)
pixels(data, ho)
data, ho = surface(width=24, height=47)
before = list(data)
lib.console_init(C.byref(ho))
lib.console_write(b'ignored')
assert list(data) == before
lib.console_init(None)
lib.console_write(b'ignored')
lib.console_clear()
print('PASS: console cursor, wrapping, backspace, tabs, scrolling, RGB/BGR and bounds')


# 2D operations: compare the real C implementation with snapshot semantics.
lib.fb_fill.argtypes = [C.POINTER(Handoff), C.c_uint32]
rect_args = [C.POINTER(Handoff), C.c_int32, C.c_int32,
             C.c_uint32, C.c_uint32, C.c_uint32]
lib.fb_fill_rect.argtypes = rect_args
lib.fb_outline.argtypes = rect_args
lib.fb_copy_rect.argtypes = [C.POINTER(Handoff)] + [C.c_int32]*4 + [C.c_uint32]*2
lib.fb_color.argtypes = [C.c_uint8, C.c_uint8, C.c_uint8, C.c_uint32]
lib.fb_color.restype = C.c_uint32
assert lib.fb_color(0x12, 0x34, 0x56, 0) == 0x563412
assert lib.fb_color(0x12, 0x34, 0x56, 1) == 0x123456

for x, y, w, h in [(2, 3, 7, 8), (-3, -2, 8, 8), (21, 13, 9, 8),
                    (0, 0, 0, 8), (0, 0, 8, 0), (-2**31, 0, 2**32-1, 4),
                    (2**31-1, 0, 2**32-1, 2), (-2**31, -2**31, 2**32-1, 2**32-1)]:
    for outline in (False, True):
        data, ho = surface()
        operation = lib.fb_outline if outline else lib.fb_fill_rect
        operation(C.byref(ho), x, y, w, h, FG)
        expected = {(px, py) for py in range(16) for px in range(24)
                    if x <= px < x+w and y <= py < y+h and
                    (not outline or px in (x, x+w-1) or py in (y, y+h-1))}
        assert pixels(data, ho) == expected, (outline, x, y, w, h)

data, ho = surface()
lib.fb_fill(C.byref(ho), FG)
assert len(pixels(data, ho)) == 24*16

import random
rng = random.Random(9)
cases = [(0, 0, 1, 0, 23, 16), (1, 0, 0, 0, 23, 16),
         (0, 0, 0, 1, 24, 15), (0, 1, 0, 0, 24, 15),
         (0, 0, 2, 2, 22, 14), (2, 2, 0, 0, 22, 14),
         (-2**31, 0, -2**31, 1, 2**32-1, 15),
         (2**31-1, 0, -2**31, 0, 2**32-1, 16)]
cases += [tuple(rng.randint(-30, 30) for _ in range(4)) +
          (rng.randrange(50), rng.randrange(40)) for _ in range(300)]
for sx, sy, dx, dy, w, h in cases:
    data, ho = surface()
    stride = ho.fb_pitch // 4
    for py in range(16):
        for px in range(24):
            data[1+py*stride+px] = py*24+px
    before = list(data)
    expected = before.copy()
    # Iterate visible destinations, reading only from the original snapshot.
    for py in range(16):
        for px in range(24):
            rx, ry = px-dx, py-dy
            ax, ay = sx+rx, sy+ry
            if 0 <= rx < w and 0 <= ry < h and 0 <= ax < 24 and 0 <= ay < 16:
                expected[1+py*stride+px] = before[1+ay*stride+ax]
    lib.fb_copy_rect(C.byref(ho), sx, sy, dx, dy, w, h)
    assert list(data) == expected, (sx, sy, dx, dy, w, h)
    pixels(data, ho)

for field, value in [('framebuffer', 0), ('framebuffer', 2**64-4),
                     ('fb_bpp', 24), ('fb_pitch', 97), ('fb_pitch', 4),
                     ('fb_width', 0), ('fb_height', 0)]:
    data, ho = surface()
    before = list(data)
    setattr(ho, field, value)
    lib.fb_fill(C.byref(ho), FG)
    lib.fb_fill_rect(C.byref(ho), 0, 0, 24, 16, FG)
    lib.fb_outline(C.byref(ho), 0, 0, 24, 16, FG)
    lib.fb_copy_rect(C.byref(ho), 0, 0, 1, 1, 10, 10)
    assert list(data) == before
print('PASS: 2D fills, outlines, clipping, integer limits and 308 overlapping copies')
