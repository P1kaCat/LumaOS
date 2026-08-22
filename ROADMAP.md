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

---

## Phase 1 — User Mode
**Statut : ✅ Terminé**

- [x] Ring 3
- [x] TSS / RSP0
- [x] Syscalls
- [x] Premier programme utilisateur
- [x] Transition Ring 3 → Ring 0
- [x] Scheduler + User Mode
- [x] Validation QEMU

---

## Phase 2 — Isolation mémoire
**Statut : ✅ Terminé**

- [x] Pages kernel supervisor-only
- [x] Région mémoire utilisateur
- [x] User code en Ring 3
- [x] Page fault sur accès à la mémoire kernel
- [x] Gestion propre du page fault attendu
- [x] Syscalls fonctionnels avec isolation
- [x] Validation QEMU

---

## Phase 3 — Processus
**Statut : ✅ Terminé**

- [x] Structure Process
- [x] PID
- [x] Création d'un processus utilisateur
- [x] Terminaison d'un processus
- [x] Scheduler compatible avec les processus
- [x] Syscall `exit`
- [x] Test QEMU création → exécution → terminaison
- [x] Plusieurs processus utilisateur simultanés
- [x] Espace mémoire propre à chaque processus
- [x] CR3 par processus
- [x] Stack kernel par processus
- [x] Test d'isolation inter-processus
- [x] Page fault inter-processus vérifié

---

## Phase 4 — Mémoire virtuelle avancée
**Statut : 🔄 En cours**

### Page allocation
- [x] Page allocator physique 4 KB
- [x] `alloc_page()`
- [x] `free_page()`
- [x] Réutilisation des pages libérées
- [x] Validation QEMU

### Gestion des mappings
- [x] `map_page()`
- [x] `unmap_page()`
- [x] Parcours des page tables 4-level
- [x] Allocation dynamique des niveaux de page tables
- [x] `invlpg` après modification des mappings
- [x] Validation QEMU du mapping dynamique
- [x] Test écriture/lecture via VA mappée
- [x] Test page fault après `unmap_page()`

### Isolation mémoire
- [x] CR3 séparé par processus
- [x] Page tables par processus
- [x] Protection inter-processus
- [ ] Protection mémoire complète par processus (test accès kernel → PF)
- [ ] Libération des pages d'un processus à sa terminaison

### Mémoire utilisateur
- [ ] Heap utilisateur (région dédiée, syscall `sbrk`)
- [ ] Allocation dynamique userland
- [ ] Mapping de pages utilisateur à la demande (lazy allocation)
- [ ] Page fault utilisateur exploitable (not-present vs protection)
- [ ] Stack utilisateur dynamique (région séparée, croissance par PF)

---

## Phase 5 — Syscalls & Userland
**Statut : ⬜ Non commencé**

- [ ] API syscall stable
- [ ] `write`
- [ ] `read`
- [x] `exit`
- [ ] `sleep`
- [x] `getpid`
- [x] `sbrk` (Phase 4)
- [ ] Gestion des erreurs syscall
- [ ] Gestion des fichiers
- [ ] Programme `init`
- [ ] Shell minimal
- [ ] Loader d'un programme user depuis un fichier

---

## Phase 6 — Filesystem & stockage
**Statut : ⬜ Non commencé**

- [ ] Driver disque
- [ ] VFS
- [ ] FAT32
- [ ] Lecture de fichiers
- [ ] Écriture de fichiers
- [ ] Répertoires
- [ ] Permissions
- [ ] File descriptors

---

## Phase 7 — Drivers
**Statut : ⬜ Non commencé**

- [ ] PCI
- [ ] USB
- [ ] Clavier USB
- [ ] Souris
- [ ] Stockage NVMe / SATA
- [ ] Audio
- [ ] Réseau

---

## Phase 8 — Réseau
**Statut : ⬜ Non commencé**

- [ ] Ethernet
- [ ] Driver NIC
- [ ] IP
- [ ] ARP
- [ ] UDP
- [ ] TCP
- [ ] DNS
- [ ] API réseau userland

---

## Phase 9 — Interface graphique
**Statut : ⬜ Non commencé**

- [ ] Framebuffer
- [ ] Console texte framebuffer
- [ ] Rendu 2D
- [ ] Input clavier
- [ ] Input souris
- [ ] Curseur
- [ ] Fenêtres
- [ ] Compositor
- [ ] Desktop LumaOS

---

## Phase 10 — Gaming
**Statut : ⬜ Non commencé**

- [ ] GPU virtuel
- [ ] Driver graphique
- [ ] Accélération graphique
- [ ] Vulkan
- [ ] Runtime gaming
- [ ] Gestion des performances
- [ ] Game Mode

---

## Phase 11 — Compatibilité
**Statut : ⬜ Non commencé**

- [ ] ELF complet
- [ ] Loader d'applications
- [ ] Librairie C minimale
- [ ] Compatibility layer Linux
- [ ] Compatibility layer Windows
- [ ] Support progressif des jeux

---

## Phase 12 — Hardware réel
**Statut : ⬜ Non commencé**

- [ ] Boot hardware réel
- [ ] ACPI
- [ ] APIC / SMP
- [ ] Interruptions modernes
- [ ] GPU réel
- [ ] Audio réel
- [ ] Réseau réel
- [ ] Support matériel progressif
