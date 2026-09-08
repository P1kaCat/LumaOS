"""Replay the CI configuration and exercise the framebuffer shell.

Run after make build: python3 tests/test_qemu_console.py
Uses the workflow's existing commands and all 23 expectations, with a localhost
TCP monitor for Windows/Linux portability. Saves PPM screenshots and serial logs.
"""
from pathlib import Path
import argparse
import os
import re
import shlex
import shutil
import socket
import subprocess
import textwrap
import time

ROOT = Path(__file__).resolve().parents[1]
os.chdir(ROOT)
parser = argparse.ArgumentParser()
parser.add_argument('--profile', choices=('ci', 'run'), default='ci')
parser.add_argument('--repeat-exec', action='store_true')
options = parser.parse_args()
profile = options.profile
workflow = Path('.github/workflows/build.yml').read_text(encoding='utf-8')
block = re.search(r'          qemu-system-x86_64 (\\\n[\s\S]+?) &\n', workflow)[1]
args = shlex.split(block.replace('\\\n', ' '))
if profile == 'run':
    makefile = Path('Makefile').read_text(encoding='utf-8')
    block = makefile.split('run: build', 1)[1].split('$(QEMU)', 1)[1].split('# debug', 1)[0]
    for name, value in {'OVMF_DIR': 'tools/ovmf', 'BUILD_DIR': 'build',
                        'EFI_ROOT': 'build/efi_root', 'DISK_IMG': 'build/disk.img',
                        'NVME_IMG': 'build/nvme.img'}.items():
        block = block.replace('$(' + name + ')', value)
    args = shlex.split(block.replace('\\\n', ' '))
    args += ['-display', 'none', '-no-reboot', '-monitor', 'none']
with socket.socket() as probe:
    probe.bind(('127.0.0.1', 0))
    port = probe.getsockname()[1]
args[args.index('-monitor')+1] = f'tcp:127.0.0.1:{port},server=on,wait=off'
args[args.index('-serial')+1] = 'file:build/console-serial.log'
shutil.copyfile('tools/ovmf/OVMF_VARS.fd', 'build/ovmf_vars.fd')
shutil.copyfile('build/disk.img', 'build/nvme.img')

harness = textwrap.dedent(workflow.split("python3 << 'PYEOF'\n", 1)[1]
                          .split('          PYEOF', 1)[0])
harness = harness.replace('socket.AF_UNIX', 'socket.AF_INET').replace(
    "s.connect('/tmp/qemu-mon.sock')", f"s.connect(('127.0.0.1', {port}))")
extra = '''
# Exercise the existing backspace handler, then enough output to force scrolling.
s.sendall(b'screendump build/console-before.ppm\\n')
time.sleep(1)
for key in ['h', 'e', 'l', 'x', 'backspace', 'p', 'ret']:
    send_key(key)
for command in ['help'] * 10 + ['pid', 'mem']:
    for key in list(command) + ['ret']:
        # m is at QWERTY semicolon's physical position on AZERTY.
        send_key({'m': 'semicolon', ' ': 'spc', '.': 'dot'}.get(key, key))
time.sleep(1)
s.sendall(b'screendump build/console-after.ppm\\n')
time.sleep(1)
# Mouse input must alter only the pointer and restore the exact previous frame.
for command in ['mouse_move 80 40', 'screendump build/pointer-moved.ppm',
                'mouse_button 1', 'screendump build/pointer-pressed.ppm',
                'mouse_button 0', 'mouse_move -80 -40',
                'screendump build/pointer-restored.ppm']:
    s.sendall((command + '\\n').encode())
    time.sleep(0.4)
    s.recv(4096)
'''
if options.repeat_exec:
    extra = extra.replace("['help'] * 10 + ['pid', 'mem']",
                          "['run prog.elf'] * 6 + ['help'] * 10 + ['pid', 'mem']")
harness = harness.replace('# Type "exit" + Enter', extra + '\n# Type "exit" + Enter')

serial = Path('build/console-serial.log')
# Never let output from a previous run satisfy the readiness check.
serial.unlink(missing_ok=True)
for capture in ('build/console-before.ppm', 'build/console-after.ppm',
                'build/pointer-moved.ppm', 'build/pointer-pressed.ppm',
                'build/pointer-restored.ppm'):
    Path(capture).unlink(missing_ok=True)
with open('build/console-qemu.log', 'w') as log_file:
    process = subprocess.Popen(['qemu-system-x86_64'] + args, stdout=log_file,
                               stderr=log_file,
                               creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
    try:
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError('QEMU exited: see build/console-qemu.log')
            if serial.exists() and "Type 'help' for commands" in serial.read_text(errors='replace'):
                break
            time.sleep(0.1)
        else:
            raise RuntimeError('Shell timeout: see build/console-serial.log')
        print('PASS: QEMU boot and shell', flush=True)
        exec(compile(harness, 'CI keyboard injection + console checks', 'exec'), {})
        time.sleep(5)
    finally:
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

log = serial.read_text(encoding='utf-8', errors='replace')
section = workflow.split('for marker in', 1)[1].split('\n          do', 1)[0]
markers = re.findall(r'"([^"\n]+)"', section)
assert len(markers) == 23
missing = [marker for marker in markers if marker not in log]
assert not missing, missing
assert '> cat hello.txt\n' in log and '> run prog.elf\n' in log
assert log.count('Hello from loaded program!') >= (8 if options.repeat_exec else 2), 'repeated exec failed'
assert log.count('[GFX9] info query and pointer checks passed') >= 2
assert '[GFX9] info query FAILED' not in log
assert 'helx\b \bp\nCommands:' in log, 'backspace command did not execute'
assert log.count('Commands:') >= 11 and 'PID: 3' in log
assert '> mem\nFree pages:' in log
counts = re.search(r'Free pages: before=(\d+) final=(\d+)', log)
assert counts and counts[1] == counts[2], 'physical pages leaked'


def ppm(path):
    raw = Path(path).read_bytes()
    header = re.match(rb'P6\s+(\d+)\s+(\d+)\s+255\s', raw)
    assert header, 'QEMU did not produce a P6 screenshot'
    width, height = int(header[1]), int(header[2])
    data = raw[header.end():]
    assert len(data) == width * height * 3
    return width, height, data


w, h, before = ppm('build/console-before.ppm')
w2, h2, after = ppm('build/console-after.ppm')
assert (w, h) == (w2, h2)
assert before[:w*40*3] == after[:w*40*3], 'scroll overwrote the boot title'
assert before[w*40*3:] != after[w*40*3:], 'shell output did not update the screen'
assert after[w*40*3:].count(b'\xff\xff\xff') > 1000, 'screen output missing'
print(f'PASS: 23/23 markers; pages {counts[1]} == {counts[2]}')
print('PASS: framebuffer output, backspace, help, pid, mem, scroll and preserved title')

assert '[MOUSE9] PS/2 three-byte input enabled' in log
mw, mh, moved = ppm('build/pointer-moved.ppm')
pw, ph, pressed = ppm('build/pointer-pressed.ppm')
rw, rh, restored = ppm('build/pointer-restored.ppm')
assert (mw, mh) == (pw, ph) == (rw, rh) == (w, h)
assert restored == after, 'pointer failed to restore the original screen'
changed = {(i % w, i // w) for i in range(w*h)
           if moved[i*3:i*3+3] != after[i*3:i*3+3]}
assert changed, 'mouse movement was not observed'
assert all((w//2 <= x < w//2+12 and h//2 <= y < h//2+16) or
           (w//2+80 <= x < w//2+92 and h//2+40 <= y < h//2+56)
           for x, y in changed), 'unexpected pointer position or screen damage'
assert pressed != moved, 'mouse button press was not observed'
assert all(pressed[i*3:i*3+3] == moved[i*3:i*3+3]
           for i in range(w*h) if not
           (w//2+80 <= i%w < w//2+92 and h//2+40 <= i//w < h//2+56))
print('PASS: PS/2 mouse motion, click/release and exact framebuffer restoration')

if options.repeat_exec:
    print('PASS: 8 ELF executions with reused process/address-space slots')
