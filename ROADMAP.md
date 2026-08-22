# LumaOS Roadmap

## Phase 0 — Fondations
**Statut : ✅ Terminé**

- [x] Bootloader UEFI
- [x] Chargement du kernel ELF
- [x] Handoff bootloader → kernel
- [x] Framebuffer
- [x] Memory map
- [x] GDT
- [x] IDT / ISR
- [x] PIC
- [x] Paging 4-level
- [x] Heap allocator
- [x] Timer / PIT
- [x] Clavier
- [x] Scheduler préemptif

## Phase 1 — User Mode
**Statut : ✅ Terminé**

- [x] Ring 3
- [x] TSS / RSP0
- [x] Syscalls
- [x] Premier programme utilisateur
- [x] Transition Ring 3 → Ring 0
- [x] Scheduler + User Mode
- [x] Validation QEMU

## Phase 2 — Isolation mémoire
**Statut : ✅ Terminé**

- [x] Pages kernel supervisor-only
- [x] Région mémoire utilisateur
- [x] User code en Ring 3
- [x] Page fault sur accès à la mémoire kernel
- [x] Gestion propre du page fault attendu
- [x] Syscalls fonctionnels avec isolation
- [x] Validation QEMU

## Phase 3 — Processus
**Statut : ✅ Terminé**

- [x] Structure Process
- [x] PID
- [x] Création d'un processus utilisateur
- [x] Terminaison d'un processus
- [x] Scheduler compatible avec les processus
- [x] Syscall `exit`
- [x] Test QEMU création → exécution → terminaison (code prêt, en attente de validation)
- [x] Plusieurs processus utilisateur simultanés
- [x] Espace mémoire propre à chaque processus
- [x] CR3 par processus

## Phase 4 — Mémoire virtuelle avancée
**Statut : ⬜ Non commencé**

- [ ] Page tables séparées kernel / user
- [ ] Gestion dynamique des mappings
- [ ] Page allocator
- [ ] Protection mémoire par processus
- [ ] Heap utilisateur
- [ ] Page fault utilisateur exploitable

## Phase 5 — Syscalls & Userland
**Statut : ⬜ Non commencé**

- [ ] API syscall stable
- [ ] `write`
- [ ] `read`
- [ ] `exit`
- [ ] `sleep`
- [ ] Gestion des fichiers
- [ ] Programme `init`
- [ ] Shell minimal

## Phase 6 — Filesystem & stockage
**Statut : ⬜ Non commencé**

- [ ] Driver disque
- [ ] VFS
- [ ] FAT32
- [ ] Lecture de fichiers
- [ ] Écriture de fichiers
- [ ] Répertoires

## Phase 7 — Drivers
**Statut : ⬜ Non commencé**

- [ ] PCI
- [ ] USB
- [ ] Clavier USB
- [ ] Souris
- [ ] Stockage NVMe / SATA
- [ ] Audio
- [ ] Réseau

## Phase 8 — Réseau
**Statut : ⬜ Non commencé**

- [ ] Ethernet
- [ ] IP
- [ ] UDP
- [ ] TCP
- [ ] DNS
- [ ] API réseau userland

## Phase 9 — Interface graphique
**Statut : ⬜ Non commencé**

- [ ] Framebuffer
- [ ] Rendu 2D
- [ ] Input souris
- [ ] Fenêtres
- [ ] Compositor
- [ ] Desktop LumaOS

## Phase 10 — Gaming
**Statut : ⬜ Non commencé**

- [ ] GPU virtuel
- [ ] Accélération graphique
- [ ] Vulkan
- [ ] Runtime gaming
- [ ] Gestion des performances
- [ ] Game Mode

## Phase 11 — Compatibilité
**Statut : ⬜ Non commencé**

- [ ] ELF complet
- [ ] Loader d'applications
- [ ] Compatibility layer Linux
- [ ] Compatibility layer Windows
- [ ] Support progressif des jeux

## Phase 12 — Hardware réel
**Statut : ⬜ Non commencé**

- [ ] Boot hardware réel
- [ ] ACPI
- [ ] APIC / SMP
- [ ] GPU réel
- [ ] Audio réel
- [ ] Réseau réel
- [ ] Support matériel progressif
