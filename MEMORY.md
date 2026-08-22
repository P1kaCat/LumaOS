# MEMORY.md — LumaOS

## Architecture actuelle

OS monocœur x86_64, long mode, identity mapping.

- **Bootloader** : UEFI (OVMF) → charge `kernel.elf` à `0x100000`, handoff struct (framebuffer + memory map)
- **Kernel** : freestanding, linked à `0x100000`, pas de higher-half
- **Paging** :
  - 4-level page tables
  - Initial kernel mapping en 2MB pages
  - 4GB identity-mapped
  - Pages kernel : supervisor-only (`0x83`)
  - Région user : `0x800000`
  - User pages : U/S (`0x87`)
  - PML4/PDPT nécessaires accessibles depuis Ring 3
  - Page tables séparées par processus via CR3
  - Mapping dynamique de pages 4KB en cours d'intégration
- **Page allocator** :
  - Allocation physique par pages de 4KB
  - `alloc_page()`
  - `free_page()`
  - Réutilisation des pages libérées validée en QEMU
- **Heap kernel** :
  - Bump allocator `kmalloc()`
  - Pas encore de `free()`
- **Scheduler** :
  - Round-robin préemptif
  - PIT 50Hz
  - `MAX_TASKS=8`
  - `struct task` contient notamment :
    - `rsp`
    - `pid`
    - `state`
    - `is_user`
    - `cr3`
    - `kernel_rsp`
  - Context switch en assembly (`isr.S`)
  - `iretq` pour Ring 0 et Ring 3
- **Processus** :
  - PID unique par processus
  - Création/terminaison
  - Plusieurs processus simultanés
  - CR3 propre à chaque processus
  - Page tables propres à chaque processus
  - Kernel stack propre à chaque processus
  - Isolation inter-processus validée par page fault
- **User mode** :
  - Ring 3 via `iretq`
  - CS=`0x1B`
  - SS=`0x23`
  - RSP user séparé
  - User code position-independent
  - Code copié à `0x800000`
  - Stack user à `0xA00000`
- **Syscalls** :
  - `int 0x80`
  - Vector 128
  - DPL=3
  - `0 = write_serial(ptr, len)`
  - `1 = exit()`
  - `2 = getpid()`
- **Exceptions** :
  - Page fault (#14) géré
  - Page fault Ring 3 → terminaison du processus
  - Page fault attendu utilisé pour les tests d'isolation

---

## Toolchain

- Compilateur : Clang/LLVM (`--target=x86_64-unknown-none`)
- Linker : `ld.lld`
- Assembleur : Clang integrated assembler (AT&T syntax)
  - `boot.S`
  - `isr.S`
  - `user_code.S`
- Pas de NASM
- QEMU : `qemu-system-x86_64`
- Firmware : OVMF / UEFI
- Build via GNU Make

---

## Structure du projet

```text
boot/efi/
  efi_main.c          Bootloader UEFI
  efi_types.h         Types EFI
  elf.h               Structures ELF

include/
  handoff.h            Structure handoff bootloader → kernel

kernel/
  boot.S               Entry point + stack setup
  kernel.c             Kernel main + framebuffer + init sequence
  cpu.c                GDT, IDT, TSS, PIC, exceptions, syscalls
  cpu.h                Déclarations CPU / serial
  isr.S                ISR stubs + context switching
  mem.c                Paging + heap + page allocator + mappings
  sched.c              Scheduler + process abstraction
  sched.h              Tasks + process API
  user.c               Création et initialisation des processus user
  user_code.S          Programme user position-independent
  linker.ld            Linker script (kernel à 0x100000)

tools/ovmf/
  OVMF_CODE.fd         Firmware UEFI
  OVMF_VARS.fd         Variables UEFI

Makefile                Build root + lancement QEMU
