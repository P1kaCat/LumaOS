# Toolchain & Build Architecture — LumaOS

> Document de référence. Toutes les décisions ci-dessous sont vérifiées et justifiées.

## Décisions verrouillées

| Décision | Choix | Statut |
|----------|-------|--------|
| Compilateur | LLVM/Clang + LLD | ✅ Validé |
| Assembleur | NASM (syntaxe Intel) | ✅ Validé |
| Build system | GNU Make (via MSYS2) | ✅ Validé |
| Émulateur | QEMU + OVMF (TianoCore) | ✅ Validé |
| Debugger | GDB (via QEMU gdbstub) | ✅ Validé |
| OS hôte | Windows natif | ✅ Validé |
| Langage | C + assembleur x86-64 | ✅ Validé |

---

## Installation sur Windows

### 1. LLVM/Clang + LLD
- **Source** : https://github.com/llvm/llvm-project/releases
- Télécharger `LLVM-xx.x.x-win64.exe`
- Inclut : `clang`, `lld`, `lld-link`, `llvm-objcopy`, etc.
- L'installeur ajoute LLVM au PATH automatiquement.

### 2. NASM
- **Source** : https://www.nasm.us/
- Ajouter au PATH manuellement après installation.

### 3. QEMU
- **Source** : https://qemu.weilnetz.de/ (QEMU 11.1.0, août 2026)
- ou : https://www.qemu.org/download/#windows
- Inclut : `qemu-system-x86_64`

### 4. OVMF (firmware UEFI pour QEMU)
- **Source** : https://qemu.weilnetz.de/test/ovmf/usr/share/OVMF/
- Télécharger **deux fichiers** :
  - `OVMF_CODE.fd` — firmware UEFI (code en lecture seule)
  - `OVMF_VARS.fd` — stockage des variables NVRAM (template)
- Placer dans : `tools/ovmf/`

**Important — gestion des variables UEFI :**
`OVMF_VARS.fd` est un fichier que QEMU écrit pendant l'exécution (les variables UEFI
sont persistées dedans). Il ne faut **jamais** utiliser le fichier template
directement dans QEMU, sinon il est modifié de façon permanente.

La procédure correcte : copier `OVMF_VARS.fd` avant chaque run :
```
tools/ovmf/OVMF_VARS.fd      → template original (jamais modifié)
build/ovmf_vars.fd           → copie de travail (créée par le Makefile)
```

Le Makefile fait automatiquement : `cp tools/ovmf/OVMF_VARS.fd build/ovmf_vars.fd`
avant chaque lancement de QEMU.

### 5. MSYS2 (Make, GDB, utilitaires Unix)
- **Source** : https://www.msys2.org/
- Installation :
  ```
  pacman -S make gdb
  ```
- Ajouter `C:\msys64\usr\bin` au PATH Windows (pour `make`, `gdb`, `cp`, `rm`, `mkdir`).

> **Note** : `xorriso`, `mtools`, `dd`, `mkfs.vfat` ne sont **pas nécessaires**
> pour Phase 0A. QEMU supporte les virtual FAT drives (voir plus bas).

### Résumé des outils

| Outil | Rôle | Source |
|-------|------|--------|
| `clang` | Compilateur C | LLVM installer |
| `lld-link` | Linker PE/COFF + ELF | LLVM installer |
| `nasm` | Assembleur x86-64 | nasm.us |
| `qemu-system-x86_64` | Émulateur | qemu.weilnetz.de |
| `OVMF_CODE.fd` | Firmware UEFI | qemu.weilnetz.de/ovmf |
| `OVMF_VARS.fd` | Template variables UEFI | qemu.weilnetz.de/ovmf |
| `make` | Build system | MSYS2 |
| `gdb` | Debugger | MSYS2 |
| `cp` / `rm` / `mkdir` | Utilitaires fichiers | MSYS2 (dans PATH) |

---

## Vérification technique : Clang → PE32+ UEFI

### Le problème

UEFI charge des binaires au format **PE32+** (le format d'exécutables Windows x86-64),
mais avec un sous-système spécifique dans le header PE. Un exécutable Windows normal
a `Subsystem = IMAGE_SUBSYSTEM_WINDOWS_GUI (2)` ou `IMAGE_SUBSYSTEM_WINDOWS_CUI (3)`.
Un binaire UEFI doit avoir `Subsystem = IMAGE_SUBSYSTEM_EFI_APPLICATION (10)`.

Le firmware UEFI vérifie ce champ. Si ce n'est pas 10, il refuse de charger le binaire.

### Comment Clang/LLD produit le bon format

```
  ┌─────────────────────────────────────────────────┐
  │  --target=x86_64-unknown-windows                 │
  │  → génère des object files COFF (pas ELF)        │
  │  → utilise l'ABI Microsoft x64 (RCX, RDX, R8, R9)│
  │  → wchar_t = 2 bytes par défaut (convention MS)  │
  └──────────────────────┬──────────────────────────┘
                         │
                         ▼
  ┌─────────────────────────────────────────────────┐
  │  -fuse-ld=lld-link                               │
  │  → LLD en mode PE/COFF (pas mode ELF)            │
  │  → produit un binaire PE32+                      │
  └──────────────────────┬──────────────────────────┘
                         │
                         ▼
  ┌─────────────────────────────────────────────────┐
  │  -Wl,-subsystem:efi_application                 │
  │  → LLD écrit Subsystem = 10 dans l'Optional Header│
  │  → le binaire est reconnu comme app UEFI         │
  └─────────────────────────────────────────────────┘
```

**Sources vérifiées :**
- UEFI Specification 2.9A, §2 : "UEFI uses a subset of the PE32+ image format.
  Subsystem type for EFI images: IMAGE_SUBSYSTEM_EFI_APPLICATION = 10"
- Microsoft PE Format docs : Subsystem field, valeur 10 = EFI application
- Jack Wherry (janvier 2026) : Snake game UEFI complet, testé QEMU + hardware réel
- Mike Krinkin (2020) : tutorial Clang + lld + UEFI + QEMU/OVMF
- OSDev Forum : threads multiples confirment la méthode

**Conclusion** : Le binaire produit n'est pas un "PE Windows classique". Le champ
Subsystem dans le PE Optional Header est la différence. `-Wl,-subsystem:efi_application`
force LLD à écrire la valeur 10, ce qui rend le binaire chargeable par UEFI.

### Flags de compilation — justification précise

```bash
clang \
  --target=x86_64-unknown-windows \
  -ffreestanding \
  -fshort-wchar \
  -mno-red-zone \
  -mno-stack-arg-probe \
  -O2 -Wall -Wextra \
  -c efi_main.c -o efi_main.o
```

| Flag | Nécessité | Justification précise |
|------|-----------|----------------------|
| `--target=x86_64-unknown-windows` | **Obligatoire** | Cible le format PE/COFF x86-64 avec ABI Microsoft. Sans ça, Clang produit de l'ELF avec ABI System V — inutilisable par UEFI. |
| `-ffreestanding` | **Obligatoire** | Indique à Clang qu'il n'y a pas de libc. Désactive les builtins qui supposent un runtime C standard. |
| `-fshort-wchar` | Recommandé (sécurité) | Garantit `wchar_t` = 2 bytes. **Techniquement redondant** avec `--target=x86_64-unknown-windows` car l'ABI Microsoft définit déjà `wchar_t` = 2 bytes. Mais c'est une garantie explicite : si le target change ou si le comportement évolue, le flag reste correct. À garder. |
| `-mno-red-zone` | Recommandé (sécurité) | Désactive le red zone (128 bytes sous RSP que les fonctions feuilles peuvent utiliser sans ajuster RSP). **Techniquement redondant** avec le target Windows car le red zone est une feature de l'ABI System V, pas Microsoft. Mais UEFI peut déclencher des interruptions (timer, etc.) qui écrasent ces 128 bytes. Le garder est une sécurité sans coût. |
| `-mno-stack-arg-probe` | **Obligatoire** | Sur le target Windows, Clang génère des appels à `__chkstk` pour les fonctions avec >4KB de stack. `__chkstk` est une fonction du runtime Windows qui n'existe pas en UEFI. Sans ce flag, une fonction avec un gros tableau local provoque une erreur de link. Le flag désactive cette génération. |
| `-O2` | Optionnel | Optimisation. Réduit la taille du binaire. |
| `-Wall -Wextra` | Optionnel | Warnings. Standard. |

### Flags de linkage — justification précise

```bash
clang \
  --target=x86_64-unknown-windows \
  -nostdlib \
  -fuse-ld=lld-link \
  -Wl,-entry:efi_main \
  -Wl,-subsystem:efi_application \
  -o BOOTX64.EFI efi_main.o
```

| Flag | Nécessité | Justification |
|------|-----------|---------------|
| `-nostdlib` | **Obligatoire** | Pas de lib standard. |
| `-fuse-ld=lld-link` | **Obligatoire** | Utilise LLD en mode PE/COFF. Sans ça, Clang essaie d'utiliser le linker système (link.exe ou GNU ld) qui peut ne pas être présent ou configuré pour UEFI. |
| `-Wl,-entry:efi_main` | **Obligatoire** | Définit le point d'entrée. UEFI appelle `efi_main(handle, system_table)` après avoir chargé le binaire. Sans ce flag, LLD utilise `_main` ou `main` par défaut. |
| `-Wl,-subsystem:efi_application` | **Obligatoire** | Écrit `Subsystem = 10` dans le PE Optional Header. C'est ce qui distingue un binaire UEFI d'un binaire Windows normal. Sans ça, LLD utilise le subsystem CONSOLE (3) par défaut et UEFI refuse le binaire. |

> **Note sur la version LLVM** : La forme string `-Wl,-subsystem:efi_application`
> est supportée par LLD depuis plusieurs années. Une issue Meson (#11258) a signalé
> que LLD n'accepte pas la valeur **numérique** 10 — il faut utiliser la **string**
> `efi_application`. C'est exactement ce qu'on fait.

---

## Headers UEFI : le strict minimum pour Phase 0A

On n'a pas besoin d'EDK2 complet ni de GNU-EFI. Pour le premier bootloader, on
définit manuellement les types UEFI dont on a besoin. C'est ~5 structures.

### Ce dont le premier bootloader a besoin

| Structure | Rôle | Source dans la spec UEFI |
|-----------|------|--------------------------|
| `EFI_HANDLE` | Handle passé à efi_main | §2.3.1 — c'est un `void *` |
| `EFI_STATUS` | Code de retour | §2.3.1 — c'est `UINTN` = `uint64_t` |
| `EFI_TABLE_HEADER` | Header commun à toutes les tables | §4.3 |
| `EFI_SYSTEM_TABLE` | Table passée à efi_main — accès à ConOut | §4.3 |
| `EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL` | Print + clear screen | §12.4 |

### Ce dont on n'a PAS besoin pour Phase 0A

- ❌ EFI_BOOT_SERVICES (viendra quand on chargera le kernel — Phase 0A étendue)
- ❌ EFI_RUNTIME_SERVICES (viendra beaucoup plus tard)
- ❌ EFI_LOADED_IMAGE_PROTOCOL (pour connaître sa propre adresse — Phase 0B)
- ❌ EFI_GRAPHICS_OUTPUT_PROTOCOL (framebuffer — Phase 0B)
- ❌ EFI_FILE_PROTOCOL (lecture de fichiers — Phase 0A étendue)
- ❌ EFI_MEMORY_DESCRIPTOR / memory map (Phase 0A étendue)

**Total** : un seul fichier `efi_types.h` avec ~100 lignes de définitions manuelles.
Pas de dépendance externe. Pas de copie d'EDK2.

---

## Création de l'image — sans dd, mkfs, mtools

### Le problème

Les commandes `dd`, `mkfs.vfat`, `mmd`, `mcopy` sont des outils Unix/MSYS2. Elles
ne sont pas disponibles en natif dans PowerShell/CMD. On ne veut pas de dépendance
à un script Bash ni ajouter des outils juste pour créer une image.

### La solution : QEMU virtual FAT drive

QEMU peut créer un **système de fichiers FAT virtuel** à partir d'un dossier
du host, à la volée. Pas besoin de créer une image `.img` manuellement.

```
qemu-system-x86_64 -drive file=fat:rw:build/efi_root,format=raw,media=disk
```

QEMU lit le dossier `build/efi_root/` sur le host et le présente comme un
disque FAT32 à la VM. Si le dossier contient `EFI/BOOT/BOOTX64.EFI`, OVMF
le trouve et le lance automatiquement.

**Avantages :**
- Aucun outil externe nécessaire (pas de dd, mkfs, mtools, xorriso)
- Mise à jour instantanée : il suffit de recompiler et relancer QEMU
- Fonctionne nativement sur Windows

**Inconvénient :**
- Limité à ~512 MB de contenu (largement suffisant pour le dev)
- Mode rw expérimental dans QEMU (on utilise `rw:` en écriture, mais en lecture
  seule c'est `fat:` sans `rw:`)

Pour Phase 0A, on utilise le mode lecture seule (`fat:`) car le bootloader
ne fait que s'exécuter — il n'écrit pas sur le disque.

### Structure attendue par QEMU

```
build/efi_root/
└── EFI/
    └── BOOT/
        └── BOOTX64.EFI     ← notre bootloader compilé
```

Le Makefile crée cette arborescence avec `mkdir -p` et `cp`.

### Plus tard (image réelle)

Quand on aura besoin d'une vraie image `.img` (pour tests hardware, distribution),
on ajoutera `mtools` via MSYS2 (`pacman -S mtools`) et un script de création
d'image. Mais ce n'est pas nécessaire pour Phase 0A.

---

## OVMF : où trouver les fichiers sous Windows

### Téléchargement

1. Aller sur https://qemu.weilnetz.de/test/ovmf/usr/share/OVMF/
2. Télécharger :
   - `OVMF_CODE.fd` (1.9 MB) — le firmware UEFI
   - `OVMF_VARS.fd` (128 KB) — le template de variables NVRAM
3. Placer dans `tools/ovmf/`

### Structure dans le projet

```
tools/
├── ovmf/
│   ├── OVMF_CODE.fd    # Firmware (lecture seule, jamais modifié)
│   └── OVMF_VARS.fd    # Template variables (jamais modifié)
```

### Gestion des variables dans QEMU

QEMU utilise deux devices pflash :
- `unit=0` : `OVMF_CODE.fd` — firmware, toujours en lecture seule (`readonly=on`)
- `unit=1` : copie de `OVMF_VARS.fd` — writable, stocke les variables UEFI

Le Makefile copie le template avant chaque run :
```makefile
cp tools/ovmf/OVMF_VARS.fd build/ovmf_vars.fd
```

On utilise `build/ovmf_vars.fd` dans QEMU (pas le template). Ainsi le template
original n'est jamais écrasé. Si les variables UEFI sont corrompues, il suffit
de recopier le template.

### Commande QEMU complète

```bash
qemu-system-x86_64 \
  -drive if=pflash,format=raw,unit=0,file=tools/ovmf/OVMF_CODE.fd,readonly=on \
  -drive if=pflash,format=raw,unit=1,file=build/ovmf_vars.fd \
  -drive file=fat:build/efi_root,format=raw,media=disk \
  -serial stdio \
  -display gtk
```

| Flag | Rôle |
|------|------|
| `-drive if=pflash,unit=0,...,readonly=on` | OVMF_CODE en lecture seule |
| `-drive if=pflash,unit=1,...` | Copie de OVMF_VARS en écriture |
| `-drive file=fat:build/efi_root,...` | Virtual FAT drive (dossier host) |
| `-serial stdio` | Sortie série dans le terminal |
| `-display gtk` | Fenêtre QEMU |

---

## Makefile — cibles pour Windows

Le Makefile utilise des commandes Unix fournies par MSYS2 (`cp`, `rm`, `mkdir`).
Ces commandes fonctionnent car MSYS2 est installé et dans le PATH.

```makefile
# toolchain.mk — variables partagées
CC          = clang
LD          = clang
QEMU        = qemu-system-x86_64

EFI_CFLAGS  = --target=x86_64-unknown-windows -ffreestanding -fshort-wchar \
              -mno-red-zone -mno-stack-arg-probe -O2 -Wall -Wextra \
              -Iboot/efi
EFI_LDFLAGS = --target=x86_64-unknown-windows -nostdlib -fuse-ld=lld-link \
              -Wl,-entry:efi_main -Wl,-subsystem:efi_application

BUILD_DIR   = build
EFI_ROOT    = $(BUILD_DIR)/efi_root
OVMF_DIR    = tools/ovmf

# Cibles
.PHONY: all build boot run debug clean

all: build

boot: $(BUILD_DIR)/boot/BOOTX64.EFI

$(BUILD_DIR)/boot/BOOTX64.EFI: boot/efi/efi_main.c
	@mkdir -p $(BUILD_DIR)/boot
	$(CC) $(EFI_CFLAGS) -c boot/efi/efi_main.c -o $(BUILD_DIR)/boot/efi_main.o
	$(LD) $(EFI_LDFLAGS) -o $@ $(BUILD_DIR)/boot/efi_main.o

build: boot
	@mkdir -p $(EFI_ROOT)/EFI/BOOT
	@cp $(BUILD_DIR)/boot/BOOTX64.EFI $(EFI_ROOT)/EFI/BOOT/BOOTX64.EFI

run: build
	@cp $(OVMF_DIR)/OVMF_VARS.fd $(BUILD_DIR)/ovmf_vars.fd
	$(QEMU) \
	  -drive if=pflash,format=raw,unit=0,file=$(OVMF_DIR)/OVMF_CODE.fd,readonly=on \
	  -drive if=pflash,format=raw,unit=1,file=$(BUILD_DIR)/ovmf_vars.fd \
	  -drive file=fat:$(EFI_ROOT),format=raw,media=disk \
	  -serial stdio \
	  -display gtk

debug: build
	@cp $(OVMF_DIR)/OVMF_VARS.fd $(BUILD_DIR)/ovmf_vars.fd
	$(QEMU) \
	  -drive if=pflash,format=raw,unit=0,file=$(OVMF_DIR)/OVMF_CODE.fd,readonly=on \
	  -drive if=pflash,format=raw,unit=1,file=$(BUILD_DIR)/ovmf_vars.fd \
	  -drive file=fat:$(EFI_ROOT),format=raw,media=disk \
	  -serial stdio -s -S \
	  -display gtk &

clean:
	@rm -rf $(BUILD_DIR)
```

**Utilisation depuis MSYS2 ou un terminal avec MSYS2 dans le PATH :**
```bash
make build    # Compile le bootloader + prépare le dossier efi_root
make run      # build + lance QEMU/OVMF
make debug    # build + lance QEMU avec gdbstub (-s -S)
make clean    # Nettoie build/
```

---

## Commande minimale de test (sans Makefile)

Pour vérifier que la toolchain fonctionne, sans rien d'autre :

```bash
# 1. Compiler
clang --target=x86_64-unknown-windows -ffreestanding -fshort-wchar -mno-red-zone -mno-stack-arg-probe -O2 -c efi_main.c -o efi_main.o

# 2. Linker
clang --target=x86_64-unknown-windows -nostdlib -fuse-ld=lld-link -Wl,-entry:efi_main -Wl,-subsystem:efi_application -o BOOTX64.EFI efi_main.o

# 3. Préparer le dossier pour QEMU virtual FAT
mkdir -p build/efi_root/EFI/BOOT
cp BOOTX64.EFI build/efi_root/EFI/BOOT/BOOTX64.EFI

# 4. Copier OVMF_VARS
cp tools/ovmf/OVMF_VARS.fd build/ovmf_vars.fd

# 5. Lancer QEMU
qemu-system-x86_64 \
  -drive if=pflash,format=raw,unit=0,file=tools/ovmf/OVMF_CODE.fd,readonly=on \
  -drive if=pflash,format=raw,unit=1,file=build/ovmf_vars.fd \
  -drive file=fat:build/efi_root,format=raw,media=disk \
  -serial stdio \
  -display gtk
```

### Résultat attendu

```
LumaOS bootloader - Hello from UEFI!
```

Affiché dans la fenêtre QEMU. Le bootloader boucle infiniment — c'est normal.

---

## Phases du développement

### Phase 0A — Bootloader UEFI

**Objectif :** `BOOTX64.EFI` bootable dans QEMU/OVMF qui affiche "LumaOS".

```
UEFI / OVMF
    ↓
BOOTX64.EFI
    ↓
"LumaOS"
```

**Code impliqué :**
- `boot/efi/efi_types.h` — types UEFI minimaux (5 structures)
- `boot/efi/efi_main.c` — point d'entrée, clear screen, print, hang
- `Makefile` + `toolchain.mk`

**Ce qu'on ne fait PAS :**
- Pas de kernel
- Pas de framebuffer
- Pas de drivers
- Pas de memory map
- Pas de ExitBootServices

### Phase 0B — Kernel minimal

**Objectif :** Le bootloader charge un kernel et lui donne le contrôle.

```
BOOTX64.EFI
    ↓ (charge kernel.elf en mémoire)
kernel_entry.asm
    ↓ (set up stack)
kmain(boot_info)
    ↓ (init framebuffer, print, hang)
```

**Prérequis avant de coder 0B :**
1. Phase 0A fonctionne
2. Interface de handoff définie (struct boot_info)
3. Le bootloader étendu : lit le kernel sur disque, récupère le framebuffer,
   prépare boot_info, ExitBootServices, saute au kernel

---

## Structure du repository

```
LumaOS/
├── README.md
├── ROADMAP.md
├── ARCHITECTURE.md
├── CONTRIBUTING.md
├── TOOLCHAIN.md             # Ce fichier
├── Makefile                 # Build root
├── toolchain.mk             # Variables toolchain
│
├── boot/
│   └── efi/                 # Phase 0A — Bootloader UEFI
│       ├── efi_main.c
│       └── efi_types.h       # Types UEFI minimaux
│
├── kernel/                  # Phase 0B — Kernel minimal (vide pour l'instant)
│
├── tools/
│   └── ovmf/                # Firmware OVMF (gitignored)
│       ├── OVMF_CODE.fd
│       └── OVMF_VARS.fd
│
├── build/                   # Output (gitignored)
│   ├── boot/
│   │   └── BOOTX64.EFI
│   ├── efi_root/             # Virtual FAT pour QEMU
│   │   └── EFI/BOOT/BOOTX64.EFI
│   └── ovmf_vars.fd          # Copie de travail OVMF_VARS
│
├── hal/                     # (vide pour l'instant)
├── drivers/                  # (vide pour l'instant)
├── shell/                    # (vide pour l'instant)
├── runtime/                  # (vide pour l'instant)
└── docs/
```

## .gitignore additions nécessaires

```
build/
tools/ovmf/
*.efi
*.o
*.elf
*.img
*.fd
```
