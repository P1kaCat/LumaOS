<p align="center">
  <img src="assets/logo.png" alt="LumaOS Logo" width="220">
</p>

<h1 align="center">LumaOS</h1>

<p align="center">
  <strong>A gaming-first operating system built from scratch.</strong>
</p>

<p align="center">
  <a href="https://github.com/P1kaCat/LumaOS">Repository</a>
  ·
  <a href="ROADMAP.md">Roadmap</a>
  ·
  <a href="MEMORY.md">Technical Memory</a>
</p>

---

## About

LumaOS is an experimental x86_64 operating system built from scratch
with one goal in mind:

> **Build an operating system that feels more like a gaming console than a traditional PC.**

Instead of trying to reproduce the complexity of Windows or Linux,
LumaOS is designed around a different philosophy:

**gaming first, simplicity first, performance first.**

The project is currently in early development and runs inside QEMU/OVMF
while its kernel and userspace are progressively being built.

---

## Vision

LumaOS aims to eventually provide a complete gaming-oriented environment
that boots directly into a simple, controller-friendly interface.

The long-term experience is envisioned around:

- 🎮 Controller-first navigation
- ⚡ Minimal system overhead
- 🖥️ Console-like user experience
- 🔊 Custom audio and audiovisual identity
- 🚀 Fast boot and responsive system
- 🧩 Native gaming APIs and runtime
- 🔧 Direct control over hardware
- 🛠️ A system built specifically around gaming

LumaOS is **not intended to become another general-purpose desktop OS**.

Every major component should answer one question:

> **Does this help the gaming experience?**

---

## Current Status

LumaOS is currently in **Phase 5 — Syscalls & Userland**.

The kernel already supports:

- UEFI boot
- ELF kernel loading
- x86_64 long mode
- GDT / IDT / TSS
- PIC
- Paging
- Kernel/user memory isolation
- Ring 3 user processes
- Per-process address spaces
- CR3 switching
- Kernel stacks
- Physical page allocation
- Dynamic page mapping
- Lazy user memory allocation
- Dynamic user stacks
- Kernel heap
- Preemptive scheduling
- PIT timer
- Keyboard input
- System calls
- Userland shell
- Process termination and memory cleanup

The current shell provides basic commands such as:

```text
help
pid
mem
sleep N
exit
