# LumaOS Roadmap

LumaOS is a gaming-first operating system built from scratch for x86_64.

---

## CI/CD
**Status: ✅ Operational**

- [x] GitHub Actions workflow (`.github/workflows/build.yml`)
- [x] Build job: `make clean && make` on `ubuntu-22.04` with `clang + lld`
- [x] Artifact verification: `kernel.elf`, `BOOTX64.EFI`, `disk.img`
- [x] QEMU boot test: headless `-display none`, `-no-reboot`
- [x] Serial output capture via `-serial file:serial.log`
- [x] Shell input injection via QEMU monitor (`sendkey` on unix socket)
- [x] Boot marker verification (12 markers):
  - `LumaOS`, `Kernel is alive!`, `[ATA]`, `[FAT32]`
  - `Phase 4+5 regression test passed`, `[VFS] test passed`, `[SYSCALL6] test passed`
  - `[CAT6] test passed`, `[INIT5] test passed`, `[EXEC12] test passed`
  - `[PCI7] devices:`, `[PCI7a2] BAR decode OK`
- [x] Serial log artifact upload on failure

Commits: `da6ede8` (initial workflow + outb fix), `e3915b7` (headless `-display none`), `d9c4dbe` (monitor `sendkey` injection), `e699f05` (added [VFS] + [SYSCALL6] markers), `c15678e` (added [INIT5] marker), `8065462` (added [CAT6] marker + cat sendkey)

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
- [x] Stable syscall table (IDs 0–7)
- [x] `write(0)`, `read(1)`, `exit(2)`, `getpid(3)`
- [x] `sbrk(4)`, `sleep(5)`, `yield(6)`, `getpages(7)`
- [x] Unknown syscall handling (`-1`)
- [x] Error-return validation

### Keyboard
- [x] Keyboard IRQ
- [x] Scancode Set 1
- [x] 256-byte keyboard ring buffer
- [x] Scancode → character conversion
- [x] Native French AZERTY layout (two tables: unshifted + shifted)
- [x] Shift handling and AZERTY special characters
- [x] `$` escape sequence warning fix

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
- [x] User page leak fix (free_user_pages frees stack data + PT page)
- [x] `free pages: before == final`
- [x] Full Phase 4 + Phase 5 QEMU validation

### Init → Shell process model
- [x] `init` program (PID 1, runs regression tests then spawns shell)
- [x] Clean `init → shell` separation (init_code.S + shell_code.S)
- [x] Syscall 11 (`spawn`) — init spawns shell as PID 2
- [x] `[INIT5] test passed` CI marker
- [x] User program loader from a file (ELF64 via syscall 12)

---

## Phase 6 — Filesystem & Storage
**Status: ✅ Completed**

### Layered architecture
```
Userland → Syscalls → VFS → FAT32 → Block Device → ATA/IDE → Disk
```

### Block device layer — ✅ Done
- [x] ATA/IDE PIO driver (`ata.c`/`ata.h`, LBA28, polled mode)
- [x] `ata_init()`: IDENTIFY, sector count detection
- [x] `ata_read_sector()`: PIO read 512 bytes
- [x] Sector read test in QEMU (boot signature 0x55AA)
- [x] Disk image generator (`tools/create_disk.py`, 64MB FAT32)

### FAT32 layer — ✅ Done
- [x] Packed on-disk structures (BPB + dir entry)
- [x] `fat32_init()`: BPB parsing, layout computation
- [x] `fat32_read_cluster()`: cluster read via ATA sectors
- [x] `fat32_next_cluster()`: FAT chain walk (one-sector cache)
- [x] `fat32_list_root()`: root directory listing with callback
- [x] 8.3 short name parsing
- [x] QEMU validation (HELLO.TXT, TEST.TXT listed)

### VFS layer — ✅ Done
- [x] File descriptor table (`vfs.c`/`vfs.h`, 8 slots)
- [x] `vfs_open()`: path → fd via fat32_lookup
- [x] `vfs_close()`: fd release with validation
- [x] `vfs_read()`: fd → kernel buffer, position tracking, EOF
- [x] Error codes: `VFS_ERR_NOT_FOUND`, `VFS_ERR_BAD_FD`
- [x] Kernel-space test in QEMU (HELLO.TXT read, EOF, invalid path/FD)

### Syscalls — ✅ Done
- [x] `open(path)` → fd (syscall 8, RDI=path)
- [x] `close(fd)` (syscall 9, RDI=fd)
- [x] `read(fd, buf, len)` → bytes read (syscall 10, RDI=fd, RSI=buf, RDX=len)
- [x] User pointer validation: `validate_user_ptr()` checks address range + page mappings (PRESENT/USER/WRITABLE)
- [x] Bounded string copy: `copy_str_from_user()` (max 64 bytes, page boundary checks)
- [x] Error codes: file not found, bad fd, invalid pointer
- [x] Ring 3 regression test (10 tests, `[SYSCALL6] test passed`)
- [x] Large page fix: `get_page()` handles 2MB/1GB pages (PTE_PS)

### Userland — ✅ Done
- [x] `cat hello.txt` shell command (open → read loop → write → close)
- [x] Error handling: file not found, read error, EOF
- [x] Automated `[CAT6] test passed` CI marker (init_code.S)
- [x] CI sendkey injection: `cat hello.txt` typed in shell via QEMU monitor
- [x] **ELF64 user program loader** (spawn_file / syscall 12)
  - [x] ELF64 header validation (magic, class, endianness, type, machine)
  - [x] Program header parsing (PT_LOAD segments)
  - [x] Per-segment page allocation and mapping at p_vaddr
  - [x] File data copy (p_filesz bytes from file offset)
  - [x] BSS zero-fill (p_memsz - p_filesz)
  - [x] Permission mapping (PF_R → PTE_USER, PF_W → PTE_WRITABLE, PF_X)
  - [x] Entry point: RIP = e_entry from ELF header
  - [x] Standalone ELF64 user program (userprogs/hello.S + userprog.ld)
  - [x] `run FILE` shell command (syscall 12 = exec)
  - [x] Init exec test: loads prog.elf, verifies spawn, sleeps for execution
  - [x] Multi-cluster FAT32 file support (create_disk.py)
  - [x] Automated `[EXEC12] test passed` CI marker
  - [x] CI sendkey injection: `run prog.elf` typed in shell via QEMU monitor

Commits: `828e808` (ATA+FAT32+VFS+cat), `c4fe2c2` (ELF64 loader + syscall 12), `f604616` (loader hardening + AZERTY dot fix)

---

## Phase 7 — Drivers
**Status: 🔄 In Progress**

### PCI — ✅ Done (Phase 7a.1)
- [x] PCI config space access (0xCF8/0xCFC, read32/read16/read8)
- [x] Bus 0 enumeration (32 devices × 8 functions, multifunction-aware)
- [x] Vendor/device/class/subclass/prog_if/header_type/BARs/IRQ
- [x] `pci_find_device()` and `pci_find_class()` for driver discovery
- [x] `[PCI7] devices:` CI marker
- Commit: `e592466`

### PCI BAR decode + device enable — ✅ Done (Phase 7a.2)
- [x] Config space write support (`pci_config_write32/write16`)
- [x] BAR size decoding (write 0xFFFFFFFF, read back, mask, restore)
- [x] BAR type detection (MEM32, MEM64, I/O, none)
- [x] 64-bit BAR handling (two consecutive register slots)
- [x] Device enable (Command register: IO + Memory + Bus Master)
- [x] Decoded BAR info in `pci_device` struct (`bar_addr[]`, `bar_size[]`, `bar_type[]`)
- [x] `[PCI7a2] BAR decode OK` CI marker

### Next drivers — ⬜ Planned
- [ ] e1000 NIC (networking)
- [ ] USB
- [ ] USB keyboard
- [ ] Mouse
- [ ] NVMe / SATA storage
- [ ] Audio
- [ ] Networking stack

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
