# Roadmap LumaOS

## Phase 0 — Fondations (Première milestone)

| # | Composant | Statut | Description |
|---|-----------|--------|-------------|
| 1 | Bootloader | ✅ | Bootloader UEFI minimal — affiche "LumaOS", charge kernel.elf, transmet framebuffer + memory map, exit boot services (Phase 0A + 0B) |
| 2 | Kernel minimal | ✅ | Point d'entrée du kernel, handoff bootloader→kernel, proof of life framebuffer + serial |
| 3 | Gestion mémoire | ⬜ | Allocation mémoire basique |
| 4 | Interruptions | ✅ | Gestion des interruptions (IDT, ISR) |
| 5 | Threads et processus | ⬜ | Scheduler minimal, multi-tâches cooperatif puis préemptif |

## Phase 1 — Entrées / Sorties

| # | Composant | Statut | Description |
|---|-----------|--------|-------------|
| 6 | Stockage | ⬜ | Accès disque (NVMe / SATA) |
| 7 | Clavier et souris | ⬜ | Gestion des entrées USB HID |
| 8 | Support manette | ⬜ | Manettes USB / Bluetooth |
| 9 | Interface graphique | ⬜ | Framebuffer puis compositing, shell |
| 10 | Audio | ⬜ | Sortie audio, musiques d'ambiance, sons UI |

## Phase 2 — Système

| # | Composant | Statut | Description |
|---|-----------|--------|-------------|
| 11 | Réseau | ⬜ | Stack réseau basique |
| 12 | Filesystem | ⬜ | Système de fichiers (lecture/écriture) |
| 13 | Launcher | ⬜ | Lancement de jeux/applications |
| 14 | Game Mode | ⬜ | Optimisation des ressources pour le gaming |
| 15 | Gaming Runtime | ⬜ | APIs gaming (DirectX 12 / Vulkan) |

## Phase 3 — Compatibilité & Performance

| # | Composant | Statut | Description |
|---|-----------|--------|-------------|
| 16 | Compatibility Layer | ⬜ | Couche de compatibilité pour jeux Windows/Linux |
| 17 | GPU acceleration | ⬜ | Support GPU virtuel puis réel |
| 18 | Drivers matériels | ⬜ | Drivers pour GPU, audio, réseau réels |
| 19 | Support des jeux modernes | ⬜ | Tests de compatibilité AAA |
| 20 | Optimisation | ⬜ | CPU, RAM, latence, temps de démarrage |

## Détail des phases

### Phase 0A — Bootloader UEFI ✅
- [x] Toolchain définie (LLVM/Clang + LLD + NASM + QEMU/OVMF)
- [x] Headers UEFI minimaux (efi_types.h)
- [x] efi_main() : ClearScreen + OutputString "LumaOS"
- [x] BOOTX64.EFI bootable dans QEMU/OVMF
- [x] Makefile avec `make run`

### Phase 0B — Kernel minimal ✅
- [x] Handoff struct bootloader → kernel (framebuffer + memory map)
- [x] Bootloader : lit kernel.elf depuis le disque (SimpleFileSystem)
- [x] Bootloader : parse ELF, charge segments à 0x100000
- [x] Bootloader : récupère GOP (framebuffer)
- [x] Bootloader : récupère memory map + ExitBootServices
- [x] Bootloader : saute au kernel avec handoff en RDI
- [x] Kernel : boot.asm (entry point, stack setup)
- [x] Kernel : kernel.c (handoff validation, framebuffer fill, serial output)
- [x] Linker script (kernel.elf à 0x100000)
- [x] Root Makefile (build kernel + bootloader + image + QEMU)
- [x] Test dans QEMU : framebuffer change de couleur + serial output — VALIDÉ ✅

### Phase 0C++ — Fondations kernel ✅ (GDT/IDT/PIC/Paging/Heap/Scheduler)
- [x] GDT setup (null + code64 + data segments)
- [x] PIC 8259A remapping (IRQ0-15 → vectors 32-47)
- [x] IDT setup (256 entries, exception + IRQ stubs)
- [x] ISR stubs en assembly (48 stubs: 32 exceptions + 16 IRQs)
- [x] Exception handler C (dump serial + halt)
- [x] Test QEMU : GDT + IDT + sti — VALIDÉ ✅
- [x] Page tables (4-level paging) — 4GB identity-mapped, 2MB pages
- [x] Heap allocator — bump allocator from UEFI conventional memory
- [x] Keyboard interrupts (IRQ1 scancode reader)
- [x] Scheduler (round-robin, PIT 50Hz, 3 tasks, context switch) — VALIDÉ QEMU ✅

## Stratégie de support matériel

1. QEMU / matériel virtuel — développement initial
2. GPU virtuel — interface graphique de base
3. Drivers open source — réutilisation de composants existants
4. Couches de compatibilité — APIs standardisées
5. Hardware réel — support progressif
6. NVIDIA RTX — étape avancée, pas une condition initiale

## Légende

- ⬜ Non commencé
- 🔄 En cours
- ✅ Terminé
- ⏸️ En pause

### Phase 1 — User mode / Ring 3 / Syscalls ✅
- [x] GDT: user code + data segments (DPL=3)
- [x] TSS: RSP0 kernel stack for ring 3 transitions
- [x] IDT: syscall gate at vector 128 (DPL=3, interrupt gate 0xEE)
- [x] Paging: U/S bit set on all page table entries (user-accessible)
- [x] Syscall handler: syscall 0 = write_serial(ptr, len)
- [x] User program: prints via syscall, spins with pause
- [x] enter_ring3: iretq with user CS/SS/RFLAGS
- [x] Test QEMU : Ring 3 + syscall + scheduler coexistence — VALIDÉ ✅
- [ ] Page-level isolation (separate user/kernel page tables)
- [ ] Process abstraction (PID, address space, fork/exec)


### Phase 2 — Page-level isolation ✅
- [x] Kernel pages supervisor-only (no U/S bit)
- [x] User region at 0x800000 (2MB page, U/S bit set)
- [x] Position-independent user code (RIP-relative strings in user_code.S)
- [x] Ring 3 cannot access kernel memory → #14 Page Fault
- [x] Page fault intercepted: displayed as PASS, not a crash
- [x] Syscalls still functional (Ring 0 handler accesses kernel)
- [ ] Separate user/kernel page tables (CR3 switching)
- [ ] Process abstraction (PID, fork/exec)

