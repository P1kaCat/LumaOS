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
  <a href="ARCHITECTURE.md">Architecture</a>
  ·
  <a href="TOOLCHAIN.md">Toolchain</a>
  ·
  <a href="CONTRIBUTING.md">Contributing</a>
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

LumaOS is actively under development.

The project is currently focused on **Phase 5 — Syscalls & Userland**.

For the complete development status, completed milestones, and upcoming work, see the **[Roadmap](ROADMAP.md)**.

> **Current milestone:** Building the foundations of a usable userland environment with system calls, an interactive shell, process management, and basic input/output.

---

## Quick Start

### Requirements

- Windows, Linux, or another environment capable of running the build toolchain
- Clang / LLVM
- LLD
- GNU Make
- QEMU
- OVMF / UEFI firmware

See **[TOOLCHAIN.md](TOOLCHAIN.md)** for the complete toolchain and setup details.

### Build & Run

```bash
git clone https://github.com/P1kaCat/LumaOS.git
cd LumaOS
make clean && make
make run
```

LumaOS currently boots through **UEFI/OVMF** and runs inside **QEMU**.

---

## Documentation

| Document | Description |
|----------|-------------|
| [Roadmap](ROADMAP.md) | Development phases, completed features, and upcoming milestones |
| [Architecture](ARCHITECTURE.md) | System architecture and core design |
| [Toolchain](TOOLCHAIN.md) | Compiler, assembler, linker, firmware, and development tools |
| [Contributing](CONTRIBUTING.md) | Build, test, development, and contribution guidelines |
| [Technical Memory](MEMORY.md) | Detailed notes about the current low-level implementation |

---

## Project Philosophy

LumaOS is built around a few core principles:

- **Gaming-first** — system design starts from the gaming experience.
- **Minimal** — avoid unnecessary layers and services.
- **Performance-oriented** — keep overhead low and behavior predictable.
- **Hardware-focused** — progressively move closer to direct hardware control.
- **Modular** — keep major subsystems replaceable and independently evolvable.
- **Open development** — document the architecture and development process.

---

## Contributing

LumaOS is an experimental project and contributions, ideas, testing, and technical feedback are welcome.

Before contributing, please read **[CONTRIBUTING.md](CONTRIBUTING.md)** and check the **[Roadmap](ROADMAP.md)** to understand the current priorities.

---

## License

LumaOS is open source. See the repository license for the terms of use and contribution.

---

<p align="center">
  <strong>LumaOS — Gaming, rebuilt from the ground up.</strong>
</p>
