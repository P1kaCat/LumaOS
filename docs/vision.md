# Vision LumaOS

## Mission

Faire tourner les jeux PC modernes, notamment les AAA, avec le moins d'overhead système possible tout en offrant une expérience utilisateur extrêmement simple.

## Principes

1. **Console, pas bureautique.** LumaOS ne remplace pas Windows. Il démarre directement sur une interface console.
2. **Minimalisme.** Pas de services inutiles. Chaque composant doit justifier son existence par rapport au gaming.
3. **Identité propre.** L'interface s'inspire de consoles comme la 3DS/Switch mais a sa propre identité audiovisuelle.
4. **Commencer petit.** D'abord QEMU et du hardware virtuel. Le hardware réel viendra progressivement.
5. **IA-first.** Le développement est assisté par l'IA pendant la phase expérimentale.

## Interface

- Bibliothèque de jeux
- Jeux récemment utilisés
- Launcher
- Paramètres
- Store éventuel
- Gestion du compte
- Gestion des périphériques
- Informations système
- Game Mode

## Ambiance audiovisuelle

- Musique d'ambiance par menu
- Sons de navigation
- Sons au lancement de jeu
- Animations de transition
- Effets visuels légers
- Thèmes

## Objectif gaming

Compatibilité et performances proches des plateformes existantes tout en supprimant l'overhead inutile. À terme : optimisation CPU, RAM, latence, démarrage, priorité aux jeux.

## Le problème des drivers

Le support GPU (notamment NVIDIA RTX) est une étape avancée. Ne pas partir du principe qu'on écrit un driver NVIDIA complet immédiatement. Commencer sur QEMU, framebuffer, GPU virtuel.
