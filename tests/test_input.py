"""Exercise the actual bounded event queue, including overflow and reset."""
import ctypes as C
import os
from pathlib import Path
import subprocess
root = Path(__file__).resolve().parents[1]
out = root / 'build/tests'
out.mkdir(parents=True, exist_ok=True)
libfile = out / ('input.dll' if os.name == 'nt' else 'input.so')
cmd = ['clang', '-O2', '-ffreestanding', '-shared', '-nostdlib',
       '-Wall', '-Wextra', '-Werror', str(root/'kernel/input.c'), '-o', str(libfile)]
cmd += (['-fuse-ld=lld', '-Wl,/noentry', '-Wl,/export:input_reset',
         '-Wl,/export:input_push', '-Wl,/export:input_read'] if os.name == 'nt' else ['-fPIC'])
subprocess.run(cmd, check=True)
lib = C.CDLL(str(libfile))
class Event(C.Structure):
    _fields_ = [('type', C.c_uint32), ('code', C.c_uint32), ('x', C.c_int32),
                ('y', C.c_int32), ('value', C.c_uint32), ('flags', C.c_uint32)]
assert C.sizeof(Event) == 24
lib.input_push.argtypes = [C.POINTER(Event)]
lib.input_read.argtypes = [C.POINTER(Event), C.c_uint32]
buf = (Event*16)()
lib.input_reset()
assert lib.input_read(buf, 1) == 0
for i in range(64):
    lib.input_push(C.byref(Event(1, i, 0, 0, i, 1)))
assert lib.input_read(buf, 0) == -1
assert lib.input_read(buf, 17) == -1
assert lib.input_read(None, 1) == -1
for batch in range(4):
    assert lib.input_read(buf, 16) == 16
    assert [e.code for e in buf] == list(range(batch*16, (batch+1)*16))
assert lib.input_read(buf, 1) == 0
lib.input_push(C.byref(Event(2, 1, 34, -4, 0, 0)))
for i in range(64): lib.input_push(C.byref(Event(1, i, 0, 0, 0, 1)))
assert lib.input_read(buf, 16) == 2
assert (buf[0].type, buf[0].value, buf[0].x, buf[0].y, buf[0].code) == (3, 64, 34, -4, 1)
assert buf[1].code == 63
lib.input_reset()
assert lib.input_read(buf, 1) == 0
print('PASS: input FIFO, empty/invalid reads, overflow resynchronization and reset')
