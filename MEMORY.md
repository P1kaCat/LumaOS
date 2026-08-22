# MEMORY.md — LumaOS

## Architecture actuelle

OS monocœur x86_64, long mode, identity mapping.
- **Bootloader** UEFI (OVMF) → charge kernel.elf à 0x100000, handoff struct (framebuffer + memory map)
- **Kernel** freestanding, linked à 0x100000, pas de higher-half
- **Paging** : 4-level, 2MB pages, 4GB identity-mapped
  - Pages kernel : supervisor-only (0x83)
  - Région user : 0x800000 (2MB page, U/S=0x87)
  - PML4[0] + PDPT[0] user-accessible (pour traverser vers la région user)
- **Scheduler** : round-robin préemptif, PIT 50Hz, MAX_TASKS=8
  - struct task : rsp, pid, state (READY/TERMINATED), is_user
  - Context switch en assembly (isr.S), iretq pour Ring 0 et Ring 3
- **User mode** : Ring 3 via iretq (CS=0x1B, SS=0x23, RSP=user stack)
  - User code position-independent (RIP-relative), copié à 0x800000
  - Stack user à 0xA00000
- **Syscalls** : int 0x80 (vector 128, DPL=3)
  - 0 = write_serial(ptr, len)
  - 1 = exit() → proc_terminate + hlt (timer switch)

## Toolchain

- Compilateur : Clang/LLVM (--target=x86_64-unknown-none)
- Linker : ld.lld
- Assembleur : Clang integrated (AT&T syntax, boot.S, isr.S, user_code.S)
- Pas de NASM (remplacé par Clang pour cross-platform)
- QEMU : qemu-system-x86_64 + OVMF (UEFI)

## Structure du projet

```
boot/efi/          Bootloader UEFI (efi_main.c, efi_types.h, elf.h)
include/handoff.h  Structure de handoff bootloader → kernel
kernel/            Code kernel
  boot.S           Entry point, stack setup
  kernel.c         Main, framebuffer, init séquence
  cpu.c            GDT, IDT, TSS, PIC, exceptions, syscalls
  cpu.h            Déclarations partagées (serial_puts, etc.)
  isr.S            ISR stubs (48), context switch
  mem.c            Paging, heap allocator (bump)
  sched.c          Scheduler round-robin, process abstraction
  sched.h          struct task, proc_*, sched_*
  user.c           user_init() → copie code, crée process
  user_code.S      Code user position-independent
  linker.ld        Linker script (kernel à 0x100000)
tools/ovmf/        OVMF firmware pour QEMU
Makefile           Build root (make run → build + QEMU)
```

## Fonctionnalités validées (test QEMU ✅)

- Phase 0 : Boot UEFI, kernel ELF, handoff, GDT/IDT/PIC, paging, heap, scheduler, clavier
- Phase 1 : Ring 3, syscalls (int 0x80), TSS/RSP0, scheduler + user coexistence
- Phase 2 : Page-level isolation (kernel supervisor-only, user @0x800000), #14 sur accès kernel, page fault intercepté comme PASS

## Phase actuelle

**Phase 3 — Processus** 🔄 En cours
- Code écrit : PID, proc_create_user, proc_terminate, syscall exit, sched_tick skip TERMINATED
- **Non encore validé en QEMU** : test création → exécution → terminaison

## Prochaines étapes

1. Valider Phase 3 en QEMU (make run → user process crée, tourne, exit, scheduler continue)
2. Marquer Phase 3 ✅ dans ROADMAP après validation
3. Phase 4 : Mémoire virtuelle avancée (page tables séparées, CR3 par processus)

## Commandes

```bash
git pull
make clean && make run   # build complet + QEMU
make run                 # build incrémental + QEMU
```

## Problèmes connus / warnings

- `serial_putc` unused dans mem.c (ignoré, sans impact)
- User region à 0x800000 (pas 0x40000000) car QEMU default RAM < 1GB
- Un seul process user à la fois (TSS RSP0 partagé — pas de kernel stack par process)
- user_code.S doit rester position-independent (RIP-relative)
- serial_puts défini dans cpu.c (non-static), redéclaré static dans mem.c (intentionnel, pas de conflit)
