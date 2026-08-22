# LumaOS Roadmap

LumaOS is a gaming-first operating system built from scratch for x86_64.

---

## Phase 0 — Foundations
**Status: ✅ Completed**

- [x] UEFI bootloader
- [x] ELF kernel loading
- [x] Bootloader → kernel handoff
- [x] Framebuffer and memory map
- [x] GDT / IDT / ISR / PIC
- [x] 4-level paging
- [x] Kernel heap allocator
- [x] PIT timer
- [x] Keyboard input
- [x] Preemptive scheduler

---

## Phase 1 — User Mode
**Status: ✅ Completed**

- [x] Ring 3 execution
- [x] TSS / RSP0
- [x] System calls
- [x] First user program
- [x] Ring 3 → Ring 0 transitions
- [x] Scheduler + user mode
- [x] QEMU validation

---

## Phase 2 — Memory Isolation
**Status: ✅ Completed**

- [x] Supervisor-only kernel pages
- [x] User memory region
- [x] User code in Ring 3
- [x] Kernel-access page fault protection
- [x] Expected page fault handling
- [x] Isolated syscalls
- [x] QEMU validation

---

## Phase 3 — Processes
**Status: ✅ Completed**

- [x] Process structure and PID management
- [x] User process creation and termination
- [x] Process-aware scheduler
- [x] `exit` syscall
- [x] Multiple simultaneous user processes
- [x] Per-process address spaces
- [x] Per-process CR3 and kernel stack
- [x] Inter-process isolation
- [x] Inter-process page fault validation

---

## Phase 4 — Advanced Virtual Memory
**Status: ✅ Completed**

### Physical memory
- [x] 4 KB physical page allocator
- [x] `alloc_page()` / `free_page()`
- [x] Page reuse validation

### Virtual memory
- [x] `map_page()` / `unmap_page()`
- [x] 4-level page table traversal
- [x] Dynamic page table allocation
- [x] `invlpg` after mapping changes
- [x] Dynamic mapping read/write tests
- [x] Page fault after unmapping

### Process memory
- [x] Separate CR3 and page tables
- [x] Inter-process memory protection
- [x] User page cleanup on termination
- [x] Memory leak detection
- [x] Kernel/user PID collision fix

### User memory
- [x] User heap and `sbrk`
- [x] Lazy allocation through page faults
- [x] Not-present vs. protection-violation handling
- [x] Dynamic user stack
- [x] Stack growth through page faults

---

## Phase 5 — Syscalls & Userland
**Status: ✅ Completed**

### Syscall API
- [x] Stable syscall table
- [x] `write`, `read`, `exit`, `getpid`
- [x] `sbrk`, `sleep`, `yield`, `getpages`
- [x] Unknown syscall handling (`-1`)
- [x] Error-return validation

### Keyboard
- [x] Keyboard IRQ
- [x] Scancode Set 1
- [x] 256-byte keyboard ring buffer
- [x] Scancode → character conversion
- [x] Native French AZERTY layout
- [x] Shift handling and AZERTY special characters

### Scheduler & timing
- [x] `SLEEPING` task state
- [x] Global `system_ticks`
- [x] Automatic wake-up
- [x] `sleep(ticks)` / `yield()`
- [x] 50 Hz timer

### Shell
- [x] Ring 3 interactive shell
- [x] Character echo and backspace
- [x] Command input and parsing
- [x] `help`, `pid`, `mem`, `sleep N`, `exit`
- [x] Numeric `itoa`
- [x] AZERTY validation
- [x] RAX/AL echo clobber bug fix

### Regression testing
- [x] Phase 4 regression tests
- [x] Shell termination test
- [x] User page leak fix
- [x] `free pages: before == final`
- [x] Full Phase 4 + Phase 5 QEMU validation

### Remaining userland work
- [ ] `init` program
- [ ] User program loader from a file
- [ ] Clean `init → shell` separation

---

## Phase 6 — Filesystem & Storage
**Status: ⬜ Not started**

- [ ] Disk driver
- [ ] VFS
- [ ] FAT32
- [ ] File read/write
- [ ] Directories
- [ ] Permissions
- [ ] File descriptors

---

## Phase 7 — Drivers
**Status: ⬜ Not started**

- [ ] PCI
- [ ] USB
- [ ] USB keyboard
- [ ] Mouse
- [ ] NVMe / SATA storage
- [ ] Audio
- [ ] Networking

---

## Phase 8 — Networking
**Status: ⬜ Not started**

- [ ] Ethernet / NIC driver
- [ ] IP / ARP
- [ ] UDP / TCP
- [ ] DNS
- [ ] Userland networking API

---

## Phase 9 — Graphical Interface
**Status: ⬜ Not started**

- [ ] Framebuffer console
- [ ] 2D rendering
- [ ] Keyboard and mouse input
- [ ] Cursor
- [ ] Windows
- [ ] Compositor
- [ ] LumaOS desktop

---

## Phase 10 — Gaming
**Status: ⬜ Not started**

- [ ] Virtual GPU
- [ ] Graphics driver
- [ ] Hardware acceleration
- [ ] Vulkan
- [ ] Gaming runtime
- [ ] Performance management
- [ ] Game Mode

---

## Phase 11 — Compatibility
**Status: ⬜ Not started**

- [ ] Full ELF support
- [ ] Application loader
- [ ] Minimal C library
- [ ] Linux compatibility layer
- [ ] Windows compatibility layer
- [ ] Progressive game support

---

## Phase 12 — Real Hardware
**Status: ⬜ Not started**

- [ ] Physical hardware boot
- [ ] ACPI
- [ ] APIC / SMP
- [ ] Modern interrupt handling
- [ ] Real GPU / audio / networking
- [ ] Progressive hardware support
