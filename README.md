# LumaOS

> Un PC qui se comporte comme une console.

LumaOS est un système d'exploitation PC entièrement pensé pour le gaming. Son objectif n'est pas de remplacer Windows pour la bureautique, mais de faire tourner des jeux PC modernes avec le moins d'overhead système possible tout en offrant une expérience utilisateur extrêmement simple.

## Vision

- Démarrer directement sur une interface de type console
- Aucune exposition inutile à la complexité d'un OS PC traditionnel
- Interface entièrement navigable à la manette, clavier et souris
- Identité audiovisuelle propre (musiques d'ambiance, sons de navigation, animations)
- Minimaliste, légère, fluide

## Architecture

```
                 Jeux / Applications
                         │
                         ▼
             Gaming Runtime / APIs
                         │
                         ▼
                 LumaOS Userland
                         │
                         ▼
                   LumaOS Kernel
                         │
                         ▼
             Hardware Abstraction Layer
                         │
             ┌───────────┼───────────┐
             ▼           ▼           ▼
            GPU         CPU         Devices
             │
             ▼
          Hardware
```

Voir [ARCHITECTURE.md](ARCHITECTURE.md) pour les détails.

## Roadmap

Le développement suit une approche progressive. Voir [ROADMAP.md](ROADMAP.md) pour le roadmap complet.

**Première milestone :**
```
UEFI → Bootloader → Kernel → Framebuffer → Shell → Home → Musique → Animations → Navigation
```

## Philosophie

- **Ne pas recréer Windows.** Chaque composant doit répondre à la question : est-ce nécessaire pour le gaming ?
- **IA-first.** Pendant la phase expérimentale, le développement est réalisé avec l'aide de l'IA.
- **Commencer petit.** D'abord fonctionner dans un environnement contrôlé (QEMU), puis évoluer vers du matériel réel.

## Statut

Projet personnel expérimental. Pas de distribution publique prévue dans l'immédiat.
