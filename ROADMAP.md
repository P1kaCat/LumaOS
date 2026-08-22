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
**Statut : ✅ Terminé**

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
- [x] Protection mémoire complète par processus
- [x] Libération des pages d'un processus à sa terminaison
- [x] Détection de fuite mémoire
- [x] Correction des collisions PID kernel/user

### Mémoire utilisateur
- [x] Heap utilisateur
- [x] `sbrk`
- [x] Allocation dynamique userland
- [x] Lazy mapping via page fault
- [x] Distinction page fault not-present / protection violation
- [x] Stack utilisateur dynamique
- [x] Croissance de stack via page fault

---

## Phase 5 — Syscalls & Userland
**Statut : ✅ Terminé**

### API syscall
- [x] Table syscall stabilisée
- [x] `write`
- [x] `read`
- [x] `exit`
- [x] `getpid`
- [x] `sbrk`
- [x] `sleep`
- [x] `yield`
- [x] `getpages`
- [x] Gestion des syscalls inconnus (`-1`)
- [x] Validation complète de tous les retours d'erreur

### Clavier
- [x] IRQ clavier
- [x] Scancode Set 1
- [x] Ring buffer clavier 256 octets
- [x] Conversion scancode → caractère
- [x] Support correct du layout AZERTY
- [x] Gestion complète des touches avec Shift
- [x] Gestion propre des caractères spéciaux AZERTY

### Scheduler / timing
- [x] État `SLEEPING`
- [x] `system_ticks`
- [x] Réveil automatique des processus
- [x] `sleep(ticks)`
- [x] `yield()`
- [x] Timer 50 Hz

### Shell
- [x] Shell userland Ring 3
- [x] Prompt interactif
- [x] Echo caractère par caractère
- [x] Backspace
- [x] Entrée / validation de commande
- [x] `help`
- [x] `pid`
- [x] `mem`
- [x] `sleep N`
- [x] `exit`
- [x] Conversion numérique `itoa`
- [x] Parsing des commandes
- [x] Validation complète du clavier AZERTY
- [x] Correction du bug RAX/AL (echo syscall clobber)

### Mémoire / régression
- [x] Régression Phase 4 depuis le shell
- [x] Test de terminaison du shell
- [x] Corriger le leak de pages à la terminaison du shell
- [x] Valider `free pages: before == final`
- [x] Validation QEMU complète Phase 4 + Phase 5

### Userland
- [ ] Programme `init`
- [ ] Loader d'un programme user depuis un fichier
- [ ] Première séparation propre init → shell

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
