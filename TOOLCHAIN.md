# LumaOS Toolchain & Build Architecture

This document defines the development toolchain used by LumaOS and the reasoning behind the major choices.

## Locked Toolchain

| Component | Choice | Status |
|---|---|---|
| C compiler | LLVM / Clang | ✅ Validated |
| Linker | LLD | ✅ Validated |
| Assembly | Clang integrated assembler, x86-64 AT&T syntax | ✅ Validated |
| Build system | GNU Make via MSYS2 | ✅ Validated |
| Emulator | QEMU `qemu-system-x86_64` | ✅ Validated |
| Firmware | OVMF / TianoCore UEFI | ✅ Validated |
| Debugger | GDB via QEMU gdbstub | Available |
| Host OS | Windows | ✅ Validated |
| Languages | C + x86-64 assembly | ✅ Validated |

> **Note:** LumaOS currently uses Clang's integrated assembler. NASM is not required by the current build system.

---

## Windows Setup

### LLVM / Clang / LLD

Install LLVM for Windows and make sure `clang` and `ld.lld` are available from the build environment.

### MSYS2

MSYS2 provides GNU Make and common Unix utilities used by the Makefiles.

```bash
pacman -S make gdb
```

### QEMU

Install QEMU for Windows and make sure `qemu-system-x86_64` is available in `PATH`.

### OVMF

Place the UEFI firmware files in:

```text
tools/ovmf/
├── OVMF_CODE.fd
└── OVMF_VARS.fd
```

`OVMF_CODE.fd` is used read-only. `OVMF_VARS.fd` is the NVRAM template; QEMU writes to a working copy at `build/ovmf_vars.fd` so the original template remains untouched.

---

## Build & Run

From the repository root:

```bash
make clean
make
make run
```

The build produces a kernel ELF, a UEFI bootloader and a working OVMF variable store under `build/`.

QEMU exposes `build/efi_root` as a virtual FAT drive and boots the UEFI application through OVMF.

---

## Kernel Compilation

The kernel is a freestanding x86_64 binary with no standard C library or operating-system runtime.

Typical kernel flags include:

```text
--target=x86_64-unknown-none
-ffreestanding
-nostdlib
-O2
-Wall
-Wextra
-mno-red-zone
-fno-pic
-fno-stack-protector
```

Key properties:

- `--target=x86_64-unknown-none` targets bare-metal x86_64.
- `-ffreestanding` tells Clang that no hosted C environment exists.
- `-nostdlib` avoids standard-library/runtime dependencies.
- `-mno-red-zone` is appropriate for interrupt-driven kernel code.
- `-fno-pic` matches the current fixed-address kernel layout.
- `-fno-stack-protector` avoids compiler-generated runtime dependencies.

The kernel is linked at `0x100000` using `kernel/linker.ld`.

---

## UEFI Bootloader

The bootloader is a UEFI application loaded by OVMF. Its responsibilities include:

1. Initial UEFI startup
2. Locating the framebuffer
3. Reading the UEFI memory map
4. Loading `kernel.elf`
5. Parsing the ELF image
6. Building the kernel handoff structure
7. Exiting UEFI boot services
8. Jumping to the kernel entry point

The bootloader and kernel use different compilation targets because the bootloader executes inside UEFI while the kernel runs directly after `ExitBootServices()`.

---

## Virtual FAT Drive

Development does not require a manually created disk image. QEMU can expose a host directory as a virtual FAT drive:

```text
-drive file=fat:rw:build/efi_root,format=raw,media=disk
```

This keeps the development loop simple:

```text
source → compile → build/efi_root → QEMU → OVMF → LumaOS
```

A real disk image can be introduced later for hardware testing and distribution.

---

## Debugging

QEMU can expose a GDB remote debugging stub when needed. This allows low-level debugging of CPU state, registers, page tables, kernel memory, interrupts, scheduler state and Ring 3 transitions.

---

## Project Structure

```text
boot/efi/
  efi_main.c       UEFI entry point
  efi_types.h      Minimal UEFI definitions
  elf.h            ELF structures

include/
  handoff.h        Bootloader → kernel handoff

kernel/
  boot.S           Kernel entry point and initial stack
  cpu.c             GDT, IDT, TSS, PIC, exceptions and syscalls
  cpu.h             CPU declarations
  isr.S             Interrupt stubs and context switching
  mem.c             Paging, heap, allocator and mappings
  sched.c           Scheduler and process abstraction
  sched.h            Task/process definitions
  user.c             User process creation
  user_code.S        Position-independent userland shell
  linker.ld          Kernel linker script

tools/ovmf/
  OVMF_CODE.fd      UEFI firmware
  OVMF_VARS.fd      NVRAM template

Makefile             Build system and QEMU launcher
```

## Design Principles

- **Minimal dependencies** — avoid unnecessary frameworks and runtimes.
- **Reproducible builds** — keep the build process explicit and deterministic.
- **Explicit low-level control** — no hidden libc dependencies in the kernel.
- **Windows-friendly development** — Windows + MSYS2 + LLVM + QEMU/OVMF.
- **Gaming-first architecture** — prioritize a small, fast and controllable system.
