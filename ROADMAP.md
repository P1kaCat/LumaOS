# Roadmap LumaOS

## Phase 0 — Fondations (Première milestone)

| # | Composant | Statut | Description |
|---|-----------|--------|-------------|
| 1 | Bootloader | ⬜ | Bootloader UEFI minimal |
| 2 | Kernel minimal | ⬜ | Point d'entrée du kernel, gestion minimale |
| 3 | Gestion mémoire | ⬜ | Allocation mémoire basique |
| 4 | Interruptions | ⬜ | Gestion des interruptions (IDT, ISR) |
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
