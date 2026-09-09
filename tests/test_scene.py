"""Exercise the compositor's real bounded dirty-region accumulator."""
import ctypes as C
import os
from pathlib import Path
import random
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'build/tests';out.mkdir(parents=True,exist_ok=True)
fixture=out/'scene_fixture.c'
fixture.write_text('#include "scene.h"\nvoid add(struct damage *d, struct rect *r) { damage_add(d,*r); }\n',encoding='utf-8')
libfile=out/('scene.dll' if os.name=='nt' else 'scene.so')
cmd=['clang','-O2','-ffreestanding','-shared','-nostdlib','-Wall','-Wextra','-Werror',
     '-I'+str(root/'userprogs'),str(fixture),'-o',str(libfile)]
cmd+=['-fuse-ld=lld','-Wl,/noentry','-Wl,/export:add'] if os.name=='nt' else ['-fPIC']
subprocess.run(cmd,check=True)
lib=C.CDLL(str(libfile))
class Rect(C.Structure): _fields_=[(n,C.c_int) for n in ('x','y','w','h')]
class Damage(C.Structure): _fields_=[('rects',Rect*16),('count',C.c_uint),('width',C.c_int),('height',C.c_int)]
lib.add.argtypes=[C.POINTER(Damage),C.POINTER(Rect)]
def points(r):
    return {(x,y) for x in range(max(0,r.x),min(128,r.x+r.w))
            for y in range(max(0,r.y),min(96,r.y+r.h))}
rng=random.Random(9)
for _ in range(100):
    d=Damage(width=128,height=96);expected=set()
    for _ in range(20):
        r=Rect(rng.randrange(-30,140),rng.randrange(-30,110),rng.randrange(-2,40),rng.randrange(-2,40))
        expected|=points(r);lib.add(C.byref(d),C.byref(r))
    actual=set()
    assert d.count<=16
    for r in d.rects[:d.count]:
        assert 0<=r.x<r.x+r.w<=128 and 0<=r.y<r.y+r.h<=96
        actual|=points(r)
    assert expected<=actual
d=Damage(width=128,height=96)
for x in range(17): lib.add(C.byref(d),C.byref(Rect(x*4,0,1,1)))
assert d.count==1 and (d.rects[0].w,d.rects[0].h)==(128,96)
for r in (Rect(2**31-1,0,2**31-1,1),Rect(-2**31,0,2**31-1,1),Rect(0,0,-1,1)):
    d=Damage(width=128,height=96);lib.add(C.byref(d),C.byref(r));assert d.count==0
print('PASS: clipped/coalesced dirty regions, 2000 random rectangles, overflow and full-frame fallback')
