# Architecture LumaOS

## Vue d'ensemble

```
┌─────────────────────────────────────────┐
│           Jeux / Applications            │
├─────────────────────────────────────────┤
│         Gaming Runtime / APIs            │
│  (DirectX 12, Vulkan, Audio, Network)    │
├─────────────────────────────────────────┤
│            LumaOS Userland               │
│  (Shell, Launcher, Game Mode, Store)    │
├─────────────────────────────────────────┤
│             LumaOS Kernel                │
│  (Scheduler, Memory, IRQ, FS, Drivers)  │
├─────────────────────────────────────────┤
│       Hardware Abstraction Layer         │
├──────────────┬──────────────┬───────────┤
│     GPU      │     CPU      │  Devices  │
│   Driver     │   Driver     │  Drivers  │
├──────────────┴──────────────┴───────────┤
│              Hardware                    │
└─────────────────────────────────────────┘
```

## Modules

### Kernel (`kernel/`)
Cœur du système. Gestion mémoire, interruptions, scheduler, syscalls.

### Bootloader (`bootloader/`)
Bootloader UEFI minimal. Initialise le matériel et charge le kernel.

### HAL (`hal/`)
Hardware Abstraction Layer. Interface unifiée entre le kernel et le matériel. Permet de remplacer/améliorer les drivers sans réécrire le système.

### Userland / Shell (`shell/`)
Interface utilisateur de type console. Home, bibliothèque, paramètres, navigation manette/clavier/souris.

### Gaming Runtime (`runtime/`)
APIs gaming : graphics (DirectX 12/Vulkan), audio, input, réseau. Couche de compatibilité pour les jeux.

### Drivers (`drivers/`)
Drivers modulaires : GPU, audio, USB, Bluetooth, NVMe, réseau. Ajoutés progressivement.

## Principes de conception

- **Modulaire** : chaque module est remplaçable indépendamment
- **Minimaliste** : pas de service inutile, pas d'overhead
- **Gaming-first** : chaque composant sert l'expérience gaming
- **Portable** : démarrer sur QEMU, évoluer vers du hardware réel

## Structure des dossiers

```
LumaOS/
├── README.md
├── ROADMAP.md
├── ARCHITECTURE.md
├── CONTRIBUTING.md
├── bootloader/      # Bootloader UEFI
├── kernel/          # Kernel LumaOS
├── hal/             # Hardware Abstraction Layer
├── shell/           # Interface utilisateur (Home, navigation)
├── runtime/         # Gaming runtime & APIs
├── drivers/         # Drivers matériels
├── docs/            # Documentation
└── tools/           # Outils de build & scripts
```
