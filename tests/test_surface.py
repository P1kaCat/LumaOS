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
           'mapping_count', 'mapping_flags', 'fixture_errors', 'surface_update', 'surface_route', 'read_pixel', 'write_pixel']
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
for limit in range(32):
    for allocation, mapping in ((limit, -1), (-1, limit // 2)):
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
    assert lib.outstanding() == 20 # draft + published snapshot
    lib.budgets(-1, -1)
    request(1, op=2, handle=handle, peer=2)
    assert lib.mapping_flags(1, 0) & 2 and not (lib.mapping_flags(2, 0) & 2)
    request(2, -1, op=2, handle=handle, peer=3)
    first, last = (2, 1) if reader_first else (1, 2)
    lib.surface_cleanup(first)
    assert lib.outstanding() == 20
    request(last, op=3, handle=handle)
    lib.surface_cleanup(last)
    clean()
    fresh = request(1, op=1, width=1, height=1)
    request(1, -1, op=3, handle=handle)
    assert fresh.handle != handle
    request(1, op=4, handle=fresh.handle)
    clean()
print('PASS: surface allocation/map rollback, unauthorized access, RO grant and both exit orders')

class Update(C.Structure):
    _fields_ = [(n, C.c_uint32) for n in ('op', 'handle', 'x', 'y', 'width', 'height', 'serial', 'reserved')]
assert C.sizeof(Update) == 32
lib.surface_update.argtypes = [C.POINTER(Update), C.c_int]
def update(pid, expected=0, **fields):
    r = Update(**fields)
    assert lib.surface_update(C.byref(r), pid) == expected, (pid, fields)
    return r
r = request(1, op=1, width=128, height=80)
h = r.handle
lib.write_pixel(1, 0, 123)
request(1, op=2, handle=h, peer=2)
assert lib.read_pixel(2, 0) == 123
lib.write_pixel(1, 0, 456)
assert lib.read_pixel(2, 0) == 123, 'draft changed published view without COMMIT'
u = update(2, 1, op=2, handle=h)
assert (u.x,u.y,u.width,u.height) == (0,0,128,80)
update(1, -2, op=1, handle=h, width=1, height=1)
update(2, -1, op=3, handle=h, serial=u.serial+1)
update(2, op=3, handle=h, serial=u.serial)
update(2, op=2, handle=h) # empty
for pid in (2,3): update(pid, -1, op=1, handle=h, width=1, height=1)
for values in ({'width':0,'height':1}, {'width':129,'height':1},
               {'x':0xffffffff,'width':2,'height':1}, {'width':1,'height':81}):
    update(1, -1, op=1, handle=h, **values)
update(1, op=1, handle=h, width=1, height=1)
assert lib.read_pixel(2,0) == 456
update(1, op=1, handle=h, x=10,y=20,width=2,height=3)
u = update(2, 1, op=2, handle=h)
assert (u.x,u.y,u.width,u.height) == (0,0,12,23)
update(3, -1, op=2, handle=h)
lib.surface_cleanup(2) # releasing a locked reader must not strand producer
update(1, op=1, handle=h, width=1,height=1)
lib.surface_cleanup(1)
clean()
print('PASS: explicit snapshot publication, bounded/coalesced damage, ACK, busy and invalid requests')

class Event(C.Structure):
    _fields_ = [('type', C.c_uint32), ('code', C.c_uint32), ('x', C.c_int32), ('y', C.c_int32), ('value', C.c_uint32), ('flags', C.c_uint32)]
class Route(C.Structure):
    _fields_ = [('op', C.c_uint32), ('handle', C.c_uint32), ('event', Event)]
assert C.sizeof(Route) == 32
lib.surface_route.argtypes = [C.POINTER(Route), C.c_int]
def route(pid, op, handle, expected=0, event=None):
    r = Route(op=op, handle=handle, event=event or Event())
    assert lib.surface_route(C.byref(r), pid) == expected, (pid,op,handle)
    return r.event
h1 = request(1, op=1,width=128,height=80).handle
h2 = request(3, op=1,width=128,height=80).handle
request(1, op=2,handle=h1,peer=2)
request(3, op=2,handle=h2,peer=2)
key = Event(type=1,code=35,value=104,flags=1)
route(1,3,h1,-1)
route(2,2,h1,-1,key) # no focus
route(2,3,h1)
assert route(1,1,h1,1).value == 1
route(2,2,h1,0,key)
assert route(1,1,h1,1).value == 104
route(3,1,h1,-1)
route(2,2,h2,-1,key)
route(2,2,h1,-1,Event(type=2,code=1,x=128,y=0))
route(2,2,h1,-1,Event(type=2,code=1,x=-1,y=0))
route(1,2,h1,-1,key)
route(2,3,h2)
assert route(1,1,h1,1).value == 0
assert route(3,1,h2,1).value == 1
for _ in range(32): route(2,2,h2,0,key)
route(2,2,h2,-2,key)
route(2,3,h1,-2) # full old queue: focus change is atomic
route(1,1,h1,0)
for _ in range(32): assert route(3,1,h2,1).value == 104
route(2,3,h1)
assert route(3,1,h2,1).value == 0
assert route(1,1,h1,1).value == 1
route(2,3,0)
assert route(1,1,h1,1).value == 0
route(2,2,h1,-1,key)
for pid in (1,2,3): lib.surface_cleanup(pid)
clean()
print('PASS: per-client input isolation, focus, coordinate bounds and atomic queue-pressure handling')
