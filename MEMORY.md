# MEMORY.md — LumaOS

## Architecture actuelle

OS monocœur x86_64, long mode, identity mapping.

- **Bootloader** : UEFI (OVMF) → charge `kernel.elf` à `0x100000`, handoff struct (framebuffer + memory map)
- **Kernel** : freestanding, linked à `0x100000`, pas de higher-half

### Paging

- 4-level page tables
- Initial kernel mapping en 2MB pages
- 4GB identity-mapped
- Pages kernel : supervisor-only (`0x83`)
- Région user : `0x800000`
- User pages : U/S (`0x87`)
- Page tables séparées par processus via CR3
- Mapping dynamique de pages 4KB
- `invlpg` après modification des mappings
- Page fault utilisateur distinguant :
  - page absente → lazy allocation
  - violation de protection → terminaison du processus

### Page allocator

- Allocation physique par pages de 4KB
- `alloc_page()`
- `free_page()`
- Réutilisation des pages libérées validée en QEMU
- `count_free_pages()`

### Heap kernel

- Bump allocator `kmalloc()`
- Pas encore de `free()`

### Mémoire utilisateur

- Heap user à `0x1000000`
- `sbrk()` pour modifier le break utilisateur
- Lazy allocation du heap via page fault
- Stack user dans la région `0xA00000-0xC00000`
- Croissance dynamique de la stack via page fault
- Libération des pages user à la terminaison
- Test de fuite mémoire présent
- **Leak de 2 pages corrigé** : `free_user_pages()` libère stack data page + PT page
- Test de mapping dynamique de Phase 4 nettoyé (PD + PT + data page freed après le test)
- `free pages: before == final` validé en QEMU

### Scheduler

- Round-robin préemptif
- PIT 50Hz
- `system_ticks`
- `MAX_TASKS=8`
- États :
  - `READY`
  - `RUNNING`
  - `SLEEPING`
  - `TERMINATED`
- `sleep(ticks)` avec réveil automatique
- `yield()` pour forcer un changement de tâche
- Context switch en assembly (`isr.S`)
- `iretq` pour Ring 0 et Ring 3

`struct task` contient notamment :
- `rsp`
- `pid`
- `state`
- `is_user`
- `cr3`
- `kernel_rsp`

### Processus

- PID unique pour les processus user
- Création / terminaison
- Plusieurs processus simultanés
- CR3 propre à chaque processus
- Page tables propres à chaque processus
- Kernel stack propre à chaque processus
- Isolation inter-processus validée
- Page fault inter-processus vérifié
- `proc_terminate()` distingue les processus user des kernel tasks afin d'éviter les collisions de PID

### User mode

- Ring 3 via `iretq`
- CS=`0x1B`
- SS=`0x23`
- RSP user séparé
- User code position-independent
- Code copié à `0x800000`
- Stack user à `0xA00000`
- Shell interactif actuellement exécuté en Ring 3

---

## Syscalls

Interface actuelle via `int 0x80`, vector 128, DPL=3.

| ID | Syscall | Fonction |
|---:|---|---|
| `0` | `write` | Écrit vers la sortie série |
| `1` | `exit` | Termine le processus |
| `2` | `getpid` | Retourne le PID courant |
| `3` | `sbrk` | Modifie le break du heap user |
| `4` | `read` | Lit le buffer clavier, non bloquant |
| `5` | `sleep` | Endort le processus pendant N ticks |
| `6` | `yield` | Force un changement de tâche |
| `7` | `getpages` | Retourne le nombre de pages physiques libres |

- Syscall inconnu → `-1`
- `sleep` : 50 ticks ≈ 1 seconde
- `read` : non bloquant, retourne `0` si aucune donnée disponible
- Le userland utilise `yield()` pour éviter le busy loop lors du polling clavier

---

## Clavier

- IRQ clavier activée
- Scancode Set 1
- Ring buffer de 256 octets
- Conversion scancode → ASCII via **deux tables AZERTY FR** :
  - `scancode_map[128]` : unshifted (lettres minuscules, & " ' ( - _ ) = ^ $ * , ; : !)
  - `scancode_shift_map[128]` : shifted (chiffres 1-0, majuscules, + % ? . /)
- Suivi de l'état Shift via scancodes 0x2A/0x36 (make) et 0xAA/0xB6 (break)
- Caractères non-ASCII (é è ç à ù ²) ignorés en unshifted, digit/équivalent ASCII en Shift
- Layout AZERTY natif, indépendant du layout Windows/QEMU
- **Validé en QEMU** : help, pid, mem, sleep, exit fonctionnent avec clavier AZERTY

### Important

Le clavier physique de développement est **AZERTY**.

---

## Shell userland

Programme actuel dans `user_code.S`, **validé en QEMU**.

Bug historique corrigé : le syscall `write` (echo) clobber RAX, ce qui détruisait le caractère dans AL avant son stockage dans `cmd_buf`. Fix : store avant echo.

Affichage au démarrage :

```text
LumaOS Shell v0.1
Type 'help' for commands

>
