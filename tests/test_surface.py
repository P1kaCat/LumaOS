"""Real surface registry: failure rollback, capabilities and view lifetimes.

QEMU additionally checks real PTE permissions and page-table reclamation.
"""
import ctypes as C
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
out = root / 'build/tests'
out.mkdir(parents=True, exist_ok=True)
libfile = out / ('surface.dll' if os.name == 'nt' else 'surface.so')
exports = ['surface_request', 'surface_cleanup', 'budgets', 'outstanding',
           'mapping_count', 'mapping_flags', 'fixture_errors']
cmd = ['clang', '-O2', '-ffreestanding', '-shared', '-nostdlib', '-Wall', '-Wextra',
       '-Werror', str(root/'kernel/surface.c'), str(root/'tests/surface_fixture.c'), '-o', str(libfile)]
cmd += (['-fuse-ld=lld', '-Wl,/noentry'] + ['-Wl,/export:'+n for n in exports]
        if os.name == 'nt' else ['-fPIC'])
subprocess.run(cmd, check=True)
lib = C.CDLL(str(libfile))
class Request(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in ('op', 'handle', 'width', 'height', 'stride', 'peer', 'index', 'reserved')] + [('address', C.c_uint64)]
assert C.sizeof(Request) == 40
lib.surface_request.argtypes = [C.POINTER(Request), C.c_int]
def request(pid, expected=0, **fields):
    r = Request(**fields)
    assert lib.surface_request(C.byref(r), pid) == expected, (pid, fields)
    return r
def clean():
    assert lib.outstanding() == 0
    assert all(lib.mapping_count(pid) == 0 for pid in range(1, 5))
    assert lib.fixture_errors() == 0

# Fail at every allocation and every mapping in a maximum-size surface.
for limit in range(16):
    for allocation, mapping in ((limit, -1), (-1, limit)):
        lib.budgets(allocation, mapping)
        request(1, -1, op=1, width=128, height=128)
        clean()
lib.budgets(-1, -1)
for reader_first in (False, True):
    r = request(1, op=1, width=128, height=80)
    handle = r.handle
    request(3, -1, op=3, handle=handle)
    request(3, -1, op=4, handle=handle)
    request(1, -1, op=2, handle=handle, peer=3)
    # Partial recipient map fails, preserving the owner's view and pages.
    lib.budgets(-1, 3)
    request(1, -1, op=2, handle=handle, peer=2)
    assert lib.mapping_count(2) == 0 and lib.mapping_count(1) == 10
    assert lib.outstanding() == 10
    lib.budgets(-1, -1)
    request(1, op=2, handle=handle, peer=2)
    assert lib.mapping_flags(1, 0) & 2 and not (lib.mapping_flags(2, 0) & 2)
    request(2, -1, op=2, handle=handle, peer=3)
    first, last = (2, 1) if reader_first else (1, 2)
    lib.surface_cleanup(first)
    assert lib.outstanding() == 10
    request(last, op=3, handle=handle)
    lib.surface_cleanup(last)
    clean()
    fresh = request(1, op=1, width=1, height=1)
    request(1, -1, op=3, handle=handle)
    assert fresh.handle != handle
    request(1, op=4, handle=fresh.handle)
    clean()
print('PASS: surface allocation/map rollback, unauthorized access, RO grant and both exit orders')
