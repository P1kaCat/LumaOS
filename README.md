<p align="center">
  <img src="assets/logo.png" alt="LumaOS Logo" width="220">
</p>

<h1 align="center">LumaOS</h1>

<p align="center">
  <strong>A gaming-first operating system built from scratch.</strong>
</p>

<p align="center">
  <a href="https://github.com/P1kaCat/LumaOS">Repository</a> ·
  <a href="ROADMAP.md">Roadmap</a> ·
  <a href="ARCHITECTURE.md">Architecture</a> ·
  <a href="TOOLCHAIN.md">Toolchain</a> ·
  <a href="CONTRIBUTING.md">Contributing</a> ·
  <a href="LICENSE">License</a>
</p>

---

## About

LumaOS is an experimental **x86_64 operating system built from scratch**, designed around one core idea:

> **Build an operating system that feels more like a gaming console than a traditional PC.**

Rather than reproducing the complexity of a general-purpose desktop operating system, LumaOS is being designed around three priorities:

**gaming first · simplicity first · performance first**

The project is currently in active early development and runs through **UEFI/OVMF and QEMU** while its kernel, memory management, processes, system calls, and userland continue to evolve.

---

## Vision

LumaOS aims to become a complete gaming-oriented operating environment that boots into a simple, fast, controller-friendly experience.

The long-term vision includes:

- 🎮 Controller-first navigation
- ⚡ Minimal system overhead
- 🖥️ Console-like user experience
- 🔊 Custom audio and audiovisual identity
- 🚀 Fast boot and responsive system behavior
- 🧩 Native gaming APIs and runtime
- 🔧 Direct hardware control
- 🛠️ A system designed specifically around gaming

LumaOS is **not intended to become another general-purpose desktop OS**.

Every major component should ultimately answer one question:

> **Does this improve the gaming experience?**

---

## Development Status

LumaOS is actively developed through a series of defined development phases.

The complete project status, completed milestones, current work, and future plans are maintained in the **[Roadmap](ROADMAP.md)** rather than duplicated here.

**→ [View the LumaOS Roadmap](ROADMAP.md)**

---

## Installation

For full setup details, prerequisites, and build instructions, see **[Quick Start](#quick-start)** below and the **[Toolchain Guide](TOOLCHAIN.md)**.

The complete list of functional and non-functional requirements is available in **[Requirements](requirements.md)** — covering all phases (0–12), the syscall ABI, memory layout, filesystem architecture, and CI/CD validation criteria.

---

## Quick Start

### Requirements

- Windows, Linux, or another environment capable of running the toolchain
- Clang / LLVM
- LLD
- GNU Make
- QEMU
- OVMF / UEFI firmware

See **[TOOLCHAIN.md](TOOLCHAIN.md)** for complete setup information.

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
| [Roadmap](ROADMAP.md) | Development phases, milestones, and future plans |
| [Architecture](ARCHITECTURE.md) | System architecture and core design |
| [Toolchain](TOOLCHAIN.md) | Compiler, assembler, linker, firmware, and development tools |
| [Contributing](CONTRIBUTING.md) | Development and contribution guidelines |
| [Requirements](requirements.md) | Functional & non-functional requirements, syscall ABI, memory layout |
| [Technical Memory](MEMORY.md) | Detailed notes on the current low-level implementation |
| [License](LICENSE) | LumaOS Community License v1.0 |

---

## Project Philosophy

LumaOS is built around a few core principles:

- **Gaming-first** — system design starts from the gaming experience.
- **Minimal** — avoid unnecessary layers and services.
- **Performance-oriented** — keep overhead low and behavior predictable.
- **Hardware-focused** — progressively move closer to direct hardware control.
- **Modular** — keep major subsystems replaceable and independently evolvable.
- **Transparent development** — document the architecture and development process.

---

## Contributing

LumaOS is an experimental project, and technical contributions, testing, bug reports, ideas, and feedback are welcome within the project's license and contribution rules.

Before contributing, please read **[CONTRIBUTING.md](CONTRIBUTING.md)** and check the **[Roadmap](ROADMAP.md)** to understand the current priorities.

> Contributions should focus on improving the Original Project. Personal modifications that are not intended as contributions must remain local unless developed as a compliant non-commercial fork or derivative project.

---

## License

LumaOS is distributed under the **LumaOS Community License v1.0**.

The license permits personal, educational, experimental, and development use, as well as contributions and compliant non-commercial forks. It also includes specific requirements concerning redistribution, attribution, project naming, branding, and commercial use.

**Commercial use and sale of LumaOS or substantially derived projects are not permitted.** Forks must use their own name and branding and must clearly reference the Original Project.

See the complete license for the legally binding terms:

**→ [LumaOS Community License v1.0](LICENSE)**

---

<p align="center">
  <strong>LumaOS — Gaming, rebuilt from the ground up.</strong>
</p>
