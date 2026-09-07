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
           str(ROOT / 'kernel/console.c'), '-o', str(lib_path)]
if os.name == 'nt':
    command += ['-fuse-ld=lld', '-Wl,/noentry', '-Wl,/export:draw_char',
                '-Wl,/export:draw_string']
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
