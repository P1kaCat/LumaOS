# LumaOS — Requirements Specification

This document defines the functional and non-functional requirements for LumaOS, an experimental x86_64 gaming-first operating system built from scratch.

Each phase has a set of requirements that must be validated (typically via QEMU tests) before the phase is considered complete.

---

## 1. Project Overview

### 1.1 Purpose

LumaOS is a hobbyist x86_64 operating system designed around three priorities:

- **Gaming first** — every component should ultimately improve the gaming experience.
- **Simplicity first** — no services or features that don't justify their existence.
- **Performance first** — minimal system overhead, fast boot, low latency.

### 1.2 Scope

LumaOS is **not** a general-purpose desktop OS. It targets a console-like user experience with controller-first navigation, native gaming APIs, and direct hardware control.

The project is developed in phases (0–12), starting from bare-metal boot and progressively building toward a full gaming environment.

### 1.3 Target Platform

| Item | Value |
|---|---|
| Architecture | x86_64 (long mode) |
| Boot method | UEFI (OVMF / TianoCore) |
| Emulator | QEMU `qemu-system-x86_64` |
| Real hardware | Future (Phase 12) |
| Languages | C (freestanding) + x86-64 assembly (AT&T syntax) |
| Toolchain | LLVM/Clang + LLD |
| Build system | GNU Make |

---

## 2. Non-Functional Requirements

### 2.1 Performance

| ID | Requirement | Priority |
|---|---|---|
| NFR-PERF-01 | Kernel boot time shall be minimized (no unnecessary initialization) | High |
| NFR-PERF-02 | Scheduler tick rate: 50 Hz (20ms quantum) | Medium |
| NFR-PERF-03 | No busy-waiting in hot paths where avoidable | Medium |
| NFR-PERF-04 | Memory allocator must support page reuse to avoid exhaustion | High |

### 2.2 Reliability & Safety

| ID | Requirement | Priority |
|---|---|---|
| NFR-REL-01 | No memory leaks: all dynamically allocated pages must be freed on process termination | Critical |
| NFR-REL-02 | Page faults on unmapped or protected memory must be caught without crashing the kernel | Critical |
| NFR-REL-03 | Inter-process memory isolation: no process can read/write another process's memory | Critical |
| NFR-REL-04 | Unknown syscalls must return -1 without crashing | High |
| NFR-REL-05 | Kernel must not depend on a hosted C runtime (fully freestanding) | Critical |

### 2.3 Maintainability

| ID | Requirement | Priority |
|---|---|---|
| NFR-MAINT-01 | Each phase must be validated via QEMU tests before proceeding to the next | Critical |
| NFR-MAINT-02 | Commit hashes must be provided and verified after modifications | High |
| NFR-MAINT-03 | Do not rewrite existing architecture unnecessarily — extend, don't replace | High |
| NFR-MAINT-04 | GitHub Actions CI must remain green after each milestone | Critical |
| NFR-MAINT-05 | Use 32-bit instructions (movl/cmpl) for 32-bit immediates in assembly to avoid sign-extension errors | High |

### 2.4 Compatibility

| ID | Requirement | Priority |
|---|---|---|
| NFR-COMP-01 | Code must compile with Clang targeting `x86_64-unknown-none` (freestanding) | Critical |
| NFR-COMP-02 | Linking must use LLD with custom linker script (`linker.ld`) | Critical |
| NFR-COMP-03 | No external dependencies at runtime — all code is self-contained | Critical |

---

## 3. Functional Requirements by Phase

### Phase 0 — Foundations ✅

| ID | Requirement | Validation |
|---|---|---|
| FR-0.01 | UEFI bootloader shall load an ELF kernel and exit boot services | QEMU boot to serial output |
| FR-0.02 | Bootloader shall pass framebuffer info and memory map to kernel via handoff struct | Serial dump of handoff fields |
| FR-0.03 | Kernel shall set up GDT, IDT, ISR, and PIC | Interrupt handling test |
| FR-0.04 | Kernel shall implement 4-level x86-64 paging | Successful boot with paging enabled |
| FR-0.05 | Kernel shall have a heap allocator (kmalloc/kfree) | Allocation + free test |
| FR-0.06 | PIT timer shall generate periodic interrupts | Scheduler tick functioning |
| FR-0.07 | Keyboard IRQ shall capture scancodes | Scancode echo to serial |
| FR-0.08 | Preemptive scheduler shall switch between kernel tasks | Task switch serial output |

### Phase 1 — User Mode ✅

| ID | Requirement | Validation |
|---|---|---|
| FR-1.01 | Kernel shall execute code in Ring 3 (user mode) | User-mode serial output |
| FR-1.02 | TSS shall provide RSP0 for Ring 0 → Ring 3 transitions | Successful iretq to user mode |
| FR-1.03 | `int 0x80` syscall interface shall work from Ring 3 | Syscall return value verified |
| FR-1.04 | First user program shall execute and return | QEMU serial output |
| FR-1.05 | Scheduler shall manage user-mode tasks alongside kernel tasks | Multiple tasks running |

### Phase 2 — Memory Isolation ✅

| ID | Requirement | Validation |
|---|---|---|
| FR-2.01 | Kernel pages shall be supervisor-only (PTE_USER not set) | Page fault on user access to kernel memory |
| FR-2.02 | User memory region shall be mapped as user-accessible | Successful user read/write |
| FR-2.03 | User code shall run in Ring 3 with isolated memory | QEMU test |
| FR-2.04 | Page fault handler shall distinguish not-present vs. protection violation | Serial log of fault type |

### Phase 3 — Processes ✅

| ID | Requirement | Validation |
|---|---|---|
| FR-3.01 | Process structure shall include PID, page tables, kernel stack, state | Process table dump |
| FR-3.02 | Kernel shall create and terminate user processes | Process lifecycle test |
| FR-3.03 | Each process shall have its own CR3 and page tables | Per-process CR3 verification |
| FR-3.04 | Scheduler shall be process-aware (round-robin across processes) | Multiple processes scheduled |
| FR-3.05 | `exit` syscall shall terminate the calling process and clean up | Process exit + resource cleanup |
| FR-3.06 | Inter-process memory isolation: process A cannot access process B's memory | Page fault on cross-process access |

### Phase 4 — Advanced Virtual Memory ✅

| ID | Requirement | Validation |
|---|---|---|
| FR-4.01 | Physical page allocator shall manage 4 KB pages (alloc_page/free_page) | Page alloc + free + reuse test |
| FR-4.02 | `map_page()` shall create 4 KB VA→PA mappings in arbitrary page tables | Dynamic mapping test |
| FR-4.03 | `unmap_page()` shall remove mappings and invalidate TLB (invlpg) | Page fault after unmap |
| FR-4.04 | Page table traversal shall allocate missing PT/PD pages dynamically | Mapping in previously empty region |
| FR-4.05 | User heap shall grow via `sbrk` syscall with lazy allocation | sbrk + page fault allocation |
| FR-4.06 | User stack shall grow dynamically through page faults | Stack growth test |
| FR-4.07 | All user pages (data + PT pages) shall be freed on process termination | Free page count: before == after |
| FR-4.08 | Memory leak detection: zero net page consumption after process lifecycle | QEMU serial: "free pages: before == final" |

### Phase 5 — Syscalls & Userland ✅

| ID | Requirement | Validation |
|---|---|---|
| FR-5.01 | Stable syscall table with IDs 0–7 | All syscalls return correct values |
| FR-5.02 | `write(fd, buf, len)` — syscall 0 | Serial output from userland |
| FR-5.03 | `read(buf, len)` — syscall 1 | Keyboard input captured in userland |
| FR-5.04 | `exit(code)` — syscall 2 | Process terminates cleanly |
| FR-5.05 | `getpid()` — syscall 3 | Returns correct PID |
| FR-5.06 | `sbrk(increment)` — syscall 4 | Heap grows, returns previous break |
| FR-5.07 | `sleep(ticks)` — syscall 5 | Process sleeps and wakes up |
| FR-5.08 | `yield()` — syscall 6 | Immediate scheduler yield |
| FR-5.09 | `getpages()` — syscall 7 | Returns free page count |
| FR-5.10 | Unknown syscall ID shall return -1 | Error handling test |
| FR-5.11 | Keyboard shall use French AZERTY layout with Shift support | Two keymap tables, special chars |
| FR-5.12 | Interactive Ring 3 shell with commands: `help`, `pid`, `mem`, `sleep N`, `exit` | Shell interaction via QEMU |
| FR-5.13 | Shell shall support character echo, backspace, and command parsing | QEMU monitor sendkey injection |
| FR-5.14 | Phase 4+5 regression test must show "free pages: before == final" | Serial marker check in CI |

### Phase 6 — Filesystem & Storage 🔄

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-6.01 | ATA/IDE PIO driver (LBA28, polled mode) with IDENTIFY | Sector read: boot signature 0x55AA | ✅ |
| FR-6.02 | `ata_read_sector()` shall read 512 bytes via PIO | QEMU ATA test | ✅ |
| FR-6.03 | FAT32 read-only parser: BPB parsing, layout computation | FAT32 init serial output | ✅ |
| FR-6.04 | `fat32_read_cluster()` shall read cluster data via ATA sectors | Root dir listing | ✅ |
| FR-6.05 | `fat32_next_cluster()` shall walk FAT chain (with sector cache) | Multi-cluster file read | ✅ |
| FR-6.06 | `fat32_list_root()` shall list root directory with 8.3 names | HELLO.TXT, TEST.TXT listed | ✅ |
| FR-6.07 | VFS layer: file descriptor table with open/close/read | Kernel-space VFS test | ✅ |
| FR-6.08 | `vfs_open(path)` shall convert user path to 8.3 and look up in FAT32 | Open test files | ✅ |
| FR-6.09 | `vfs_read(fd, buf, len)` shall read from cluster chain with position tracking | Read file contents to serial | ✅ |
| FR-6.10 | `vfs_close(fd)` shall free FD slot | Close and reopen test | ✅ |
| FR-6.11 | VFS error handling: NOT_FOUND, BAD_FD, NO_FD, IO | Negative test cases | ✅ |
| FR-6.12 | `open(path)` → fd — syscall 8 | Userland file open | ⬜ |
| FR-6.13 | `close(fd)` — syscall 9 | Userland file close | ⬜ |
| FR-6.14 | `read(fd, buf, len)` → bytes read — syscall 10 | Userland file read | ⬜ |
| FR-6.15 | Syscall handler shall validate user pointers before VFS access | Invalid pointer rejection | ⬜ |
| FR-6.16 | `cat <filename>` shell command | Read and display file contents | ⬜ |
| FR-6.17 | File-based user program loader | Load ELF from disk | ⬜ |

### Phase 7 — Drivers ⬜

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-7.01 | PCI bus enumeration and device discovery | PCI device list | ⬜ |
| FR-7.02 | USB host controller driver | USB device detection | ⬜ |
| FR-7.03 | USB keyboard driver (replacing PS/2 IRQ fallback) | USB keyboard input | ⬜ |
| FR-7.04 | Mouse driver (PS/2 and/or USB) | Pointer movement events | ⬜ |
| FR-7.05 | NVMe or SATA storage driver (AHCI) | Block read/write test | ⬜ |
| FR-7.06 | Audio driver (HDA / AC97) | Sound output | ⬜ |
| FR-7.07 | Network interface driver (NIC) | Packet send/receive | ⬜ |

### Phase 8 — Networking ⬜

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-8.01 | Ethernet / NIC driver with packet TX/RX | Loopback ping | ⬜ |
| FR-8.02 | ARP protocol implementation | ARP resolution test | ⬜ |
| FR-8.03 | IP stack (IPv4) | IP packet handling | ⬜ |
| FR-8.04 | UDP / TCP sockets | Echo server test | ⬜ |
| FR-8.05 | DNS resolver | Domain name lookup | ⬜ |
| FR-8.06 | Userland networking API (socket syscalls) | User program network access | ⬜ |

### Phase 9 — Graphical Interface ⬜

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-9.01 | Framebuffer console (text rendering to screen) | On-screen text display | ⬜ |
| FR-9.02 | 2D rendering primitives (lines, rects, blits, fonts) | Graphics test | ⬜ |
| FR-9.03 | Keyboard and mouse input integration with UI | Interactive navigation | ⬜ |
| FR-9.04 | Cursor rendering and movement | Mouse cursor visible | ⬜ |
| FR-9.05 | Window management (create, move, resize, close) | Window operations | ⬜ |
| FR-9.06 | Compositor (window compositing with layers) | Overlapping windows | ⬜ |
| FR-9.07 | LumaOS desktop environment (console-like UI) | Full desktop demo | ⬜ |

### Phase 10 — Gaming ⬜

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-10.01 | Virtual GPU / graphics acceleration abstraction | GPU commands execute | ⬜ |
| FR-10.02 | Hardware-accelerated 2D/3D rendering | Frame rate benchmark | ⬜ |
| FR-10.03 | Vulkan or graphics API layer | Triangle render test | ⬜ |
| FR-10.04 | Gaming runtime (audio + input + graphics unified) | Sample game runs | ⬜ |
| FR-10.05 | Performance management (CPU/GPU scheduling for games) | Latency measurement | ⬜ |
| FR-10.06 | Game Mode (resource prioritization for foreground game) | Performance comparison | ⬜ |

### Phase 11 — Compatibility ⬜

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-11.01 | Full ELF loader (parse and load ELF executables from disk) | Load and run ELF binary | ⬜ |
| FR-11.02 | Application loader (exec from filesystem) | Launch program from shell | ⬜ |
| FR-11.03 | Minimal C library (libc subset for userland) | Compile and run C program | ⬜ |
| FR-11.04 | Linux compatibility layer (syscall translation) | Run simple Linux binary | ⬜ |
| FR-11.05 | Windows compatibility layer (Wine-like) | Run simple Windows binary | ⬜ |
| FR-11.06 | Progressive game support (increasing compatibility) | Target game launches | ⬜ |

### Phase 12 — Real Hardware ⬜

| ID | Requirement | Validation | Status |
|---|---|---|---|
| FR-12.01 | Boot on physical hardware (UEFI) | Successful hardware boot | ⬜ |
| FR-12.02 | ACPI parsing (tables, power management) | ACPI device enumeration | ⬜ |
| FR-12.03 | APIC / SMP support (multi-core) | Multi-core scheduling | ⬜ |
| FR-12.04 | Modern interrupt handling (MSI/MSI-X) | Device interrupts via MSI | ⬜ |
| FR-12.05 | Real GPU driver (framebuffer + acceleration) | On-screen display on hardware | ⬜ |
| FR-12.06 | Real audio driver | Sound output on hardware | ⬜ |
| FR-12.07 | Real networking | Network communication on hardware | ⬜ |

---

## 4. CI/CD Requirements

| ID | Requirement | Validation | Status |
|---|---|---|---|
| CI-01 | GitHub Actions workflow shall build kernel + bootloader + disk image | Build job passes | ✅ |
| CI-02 | Build job shall use Clang + LLD on Ubuntu 22.04 | Artifact verification | ✅ |
| CI-03 | QEMU boot test shall run headless (`-display none`, `-no-reboot`) | QEMU job passes | ✅ |
| CI-04 | Serial output shall be captured to file for marker verification | serial.log produced | ✅ |
| CI-05 | Shell input shall be injected via QEMU monitor (`sendkey`) | Shell commands executed | ✅ |
| CI-06 | Serial markers shall be verified: `LumaOS`, `Kernel is alive!`, `[ATA]`, `[FAT32]`, `Phase 4+5 regression test passed`, `[VFS] test passed` | All markers found | ✅ |
| CI-07 | Serial log artifact shall be uploaded on failure | Downloadable artifact | ✅ |

---

## 5. Syscall ABI Specification

### 5.1 Calling Convention

| Parameter | Register |
|---|---|
| Syscall ID | RAX |
| Arg 1 | RDI |
| Arg 2 | RSI |
| Arg 3 | RDX |
| Arg 4 | R10 |
| Return value | RAX |

Invocation: `int 0x80` from Ring 3.

### 5.2 Syscall Table

| ID | Name | Signature | Phase | Status |
|---|---|---|---|---|
| 0 | write | `int write(int fd, const char *buf, int len)` | 5 | ✅ |
| 1 | read | `int read(char *buf, int len)` | 5 | ✅ |
| 2 | exit | `void exit(int code)` | 5 | ✅ |
| 3 | getpid | `int getpid(void)` | 5 | ✅ |
| 4 | sbrk | `void *sbrk(int increment)` | 5 | ✅ |
| 5 | sleep | `void sleep(int ticks)` | 5 | ✅ |
| 6 | yield | `void yield(void)` | 5 | ✅ |
| 7 | getpages | `int getpages(void)` | 5 | ✅ |
| 8 | open | `int open(const char *path)` | 6 | ⬜ |
| 9 | close | `int close(int fd)` | 6 | ⬜ |
| 10 | read | `int read(int fd, void *buf, int len)` | 6 | ⬜ |

> Syscall IDs 0–7 are reserved. Phase 6 new syscalls start at ID 8. Existing IDs must not be renumbered.

---

## 6. Memory Layout

```
0x000000   Bootloader / UEFI
0x100000   Kernel (ELF, linked at 0x100000)
...        Kernel heap, page allocator bitmap
0x00800000  User code (Ring 3)
0x00A00000  User stack region (grows down, lazy allocation)
0x01000000  User heap (sbrk-managed, lazy allocation)
```

Kernel pages: supervisor-only (PTE_PRESENT | PTE_WRITABLE, no PTE_USER).
User pages: user-accessible (PTE_PRESENT | PTE_WRITABLE | PTE_USER).

---

## 7. Filesystem Architecture (Phase 6)

```
Userland → Syscalls (8/9/10) → VFS → FAT32 → ATA/IDE → Disk
```

### 7.1 Limitations (intentional for Phase 6 read-only)

- Read-only: no file creation, deletion, or writing
- Root directory only: no subdirectory traversal
- 8.3 short names only: no LFN (long filename) support
- No permissions, timestamps, or file attributes beyond directory entry
- Global FD table (8 slots): will become per-process when syscalls are wired in

### 7.2 Disk Image

| Parameter | Value |
|---|---|
| Size | 64 MB |
| Filesystem | FAT32 |
| Sectors per cluster | 1 (512 bytes) |
| Reserved sectors | 32 |
| Number of FATs | 2 |
| Root cluster | 2 |
| Test files | HELLO.TXT (19B), TEST.TXT (28B) |
| Generator | `tools/create_disk.py` |

---

## 8. Glossary

| Term | Definition |
|---|---|
| Ring 0 | x86 privilege level for kernel/supervisor code |
| Ring 3 | x86 privilege level for user/application code |
| CR3 | Control register holding the physical address of the PML4 page table |
| TSS | Task State Segment — stores RSP0 for kernel stack on privilege transitions |
| PML4 | Page Map Level 4 — top-level x86-64 page table |
| PT | Page Table — lowest level page table (4 KB entries) |
| PD | Page Directory — second-level page table (2 MB entries) |
| PDPT | Page Directory Pointer Table — third-level page table (1 GB entries) |
| BPB | BIOS Parameter Block — FAT filesystem metadata in the boot sector |
| LBA28 | 28-bit Logical Block Addressing for ATA/IDE devices |
| PIO | Programmed I/O — CPU-driven I/O via port reads/writes (no DMA) |
| VFS | Virtual File System — abstraction layer between syscalls and concrete filesystems |
| FD | File Descriptor — integer index into the file descriptor table |
| 8.3 | FAT filename format: 8-char name + 3-char extension |
| EOC | End Of Cluster — FAT chain terminator marker (≥ 0x0FFFFFF8) |
