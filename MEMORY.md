# MEMORY.md — LumaOS

Ce document décrit l'architecture mémoire, le système de fichiers, les appels système et l'état des sous-systèmes matériels de LumaOS.

---

## 1. Architecture Mémoire

LumaOS est un système d'exploitation 64-bit (x86_64) fonctionnant en *long mode* avec *identity mapping*.

### Boot & Kernel
- **Bootloader** : UEFI (OVMF) écrit en C (`boot/efi/efi_main.c`).
- **Kernel** : Freestanding ELF64 chargé à `0x100000` (1 Mo), sans higher-half mapping.
- **Handoff Structure** (`handoff.h`) : Transmet au kernel le framebuffer graphique (adresse, résolution, pitch, format BGR/RGB), la table de mémoire UEFI et le pointeur vers la table ACPI RSDP.

### Pagination (Paging)
- Tables de pages à 4 niveaux (PML4, PDPT, PD, PT).
- **Identity mapping** initial : 4 Go mappés en pages de 2 Mo (`0x83` = Present | Writable | Huge).
- **Pages Kernel** : Supervisor-only (`0x83` / `0x03`).
- **Région User** : À partir de `0x800000` (8 Mo) avec flags U/S (`0x87` / `0x07` = Present | Writable | User).
- **CR3 séparé** : Chaque processus possède son propre PML4 et son espace d'adressage virtuel isolé.
- **Mapping dynamique 4 Ko** : Fonctions `map_page()` et `unmap_page()` avec allocation dynamique de tables de pages intermédiaires et invalidation TLB (`invlpg`).
- **Gestion du Page Fault (`#PF`)** :
  - Page non présente dans la région heap ou stack utilisateur $\rightarrow$ Allocation paresseuse (*lazy allocation*).
  - Violation de privilège ou adresse hors limites $\rightarrow$ Terminaison du processus fautif sans crash kernel.

### Allocateur Physique (Physical Page Allocator)
- Allocation et désallocation par tranches physiques de 4 Ko (`PAGE_SIZE = 4096`).
- `alloc_page()` : Alloue une page physique libre.
- `free_page(p)` : Remet une page dans la liste chaînée des pages libres.
- `count_free_pages()` : Retourne le nombre total de pages disponibles.
- Zéro fuite mémoire vérifiée par régression (`free pages: before == final`).

### Heap Kernel
- Allocateur bump linéaire minimaliste `kmalloc(size)`.
- Initialisé à partir de la mémoire physique disponible après le kernel.

### Mémoire Utilisateur (Userland Memory Layout)
- **Code utilisateur** : Chargé à `0x800000` (Ring 3, DPL=3).
- **Stack utilisateur** : Située entre `0xA00000` et `0xC00000`, grandissant dynamiquement vers le bas via page fault.
- **Heap utilisateur** : Débute à `0x1000000`, manipulable via le syscall `sbrk`.
- **Nettoyage automatique** : `free_user_pages()` désalloue les pages de code, de pile, de heap ainsi que les tables de pages (PT/PD) associées lors de la terminaison du processus.

---

## 2. Processus et Ordonnanceur

- **Ordonnanceur préemptif Round-Robin** :
  - Fréquence PIT : 50 Hz (`system_ticks` incrémenté tous les 20 ms).
  - Tâches limitées à `MAX_TASKS = 8`.
- **États d'une tâche** :
  - `READY` : Prête à être exécutée.
  - `RUNNING` : En cours d'exécution sur le processeur.
  - `SLEEPING` : Endormie jusqu'à une échéance `wake_tick` (géré par `sleep()`).
  - `TERMINATED` : Tâche terminée, en attente de recyclage / libération des ressources.
- **Structure `task`** : Contient `rsp`, `pid`, `state`, `is_user`, `cr3`, `kernel_rsp`, `wake_tick`, etc.
- **Commutation de contexte** : Sauvegarde et restauration des registres en assembleur (`isr.S`) avec retour via `iretq`.
- **Modèle de processus** :
  - `init` (PID 1) : Exécute les tests de non-régression puis charge le shell interactif via `spawn` (syscall 11).
  - `shell` (PID 2) : Shell interactif Ring 3.

---

## 3. Appels Système (Syscalls)

Invoqués via l'interruption logicielle `int 0x80` (vecteur 128, DPL=3).
Les arguments sont passés dans les registres x86_64 standards (`RAX` = ID du syscall, `RDI`, `RSI`, `RDX`, `R10`, `R8`, `R9`).

| ID | Syscall | Paramètres | Description |
|---:|---|---|---|
| `0` | `write` | `RDI`: ptr buffer, `RSI`: len | Écrit une chaîne vers la sortie série |
| `1` | `exit` | `RDI`: exit code | Termine le processus courant et libère sa mémoire |
| `2` | `getpid` | *aucun* | Retourne le PID du processus courant |
| `3` | `sbrk` | `RDI`: incr | Ajuste le break du heap utilisateur |
| `4` | `read` | `RDI`: ptr buffer, `RSI`: max_len | Lit depuis le buffer clavier (non bloquant) |
| `5` | `sleep` | `RDI`: ticks | Endort le processus pour N ticks (50 ticks ≈ 1s) |
| `6` | `yield` | *aucun* | Cède immédiatement le processeur à une autre tâche |
| `7` | `getpages`| *aucun* | Retourne le nombre de pages physiques libres |
| `8` | `open` | `RDI`: ptr path | Ouvre un fichier FAT32 $\rightarrow$ retourne un `fd` |
| `9` | `close` | `RDI`: fd | Ferme un descripteur de fichier |
| `10`| `read` (VFS) | `RDI`: fd, `RSI`: ptr buf, `RDX`: len | Lit N octets depuis un descripteur de fichier |
| `11`| `spawn` | `RDI`: entry_point | Crée et démarre un nouveau processus utilisateur |
| `12`| `exec` | `RDI`: ptr path | Charge et exécute un binaire ELF64 depuis le disque |

*Note : Les pointeurs passés par le Ring 3 sont validés via `validate_user_ptr()` et `copy_str_from_user()` pour garantir qu'ils pointent vers des pages utilisateur valides.*

---

## 4. Clavier et Entrées

- **Matériel** : Contrôleur clavier PS/2 (IRQ 1).
- **Protocole** : Scancode Set 1.
- **Ring Buffer** : Tampon circulaire de 256 octets dans le kernel.
- **Disposition AZERTY FR native** :
  - Table sans Shift (`scancode_map`) : minuscules, ponctuation de base.
  - Table avec Shift (`scancode_shift_map`) : chiffres 1-0, majuscules, caractères complémentaires.
  - Gestion des scancodes Make/Break pour Shift gauche (`0x2A` / `0xAA`) et droit (`0x36` / `0xB6`).
  - Fonctionnement vérifié sur clavier physique AZERTY et via injection QEMU `sendkey`.

---

## 5. Stockage et Système de Fichiers (Phase 6)

### Pile de stockage
$$\text{Userland} \longrightarrow \text{Syscalls (open/close/read/exec)} \longrightarrow \text{VFS} \longrightarrow \text{FAT32} \longrightarrow \text{Driver ATA/IDE PIO} \longrightarrow \text{Disque physique/virtuel}$$

- **Driver ATA/IDE PIO** (`ata.c` / `ata.h`) :
  - Mode PIO LBA28 sur ports I/O Primaires (`0x1F0-0x1F7`, `0x3F6`).
  - Détection du nombre de secteurs via la commande `IDENTIFY` (`0xEC`).
  - Lecture secteur par secteur (`ata_read_sector`).
- **Système de fichiers FAT32** (`fat32.c` / `fat32.h`) :
  - Parsing complet du BPB (Bios Parameter Block) et de l'EBPB.
  - Gestion des clusters, chaînes FAT (FAT1/FAT2) et répertoires au format 8.3.
  - Support des fichiers multi-clusters.
  - Parcours du répertoire racine (`fat32_list_root`) et recherche de fichiers (`fat32_lookup`).
- **Couche VFS** (`vfs.c` / `vfs.h`) :
  - Table globale de 8 descripteurs de fichiers (`MAX_FDS = 8`).
  - Maintien de la position courante (`offset`), de la taille et du cluster en cours de lecture.
  - Gestion de la fin de fichier (`EOF`) et validation des accès.
- **Chargeur de binaires ELF64** (`user.c`, `elf.h`) :
  - Validation de l'en-tête ELF (Magic, 64-bit, Little Endian, x86_64, `ET_EXEC`).
  - Parsing des `Program Headers` (`PT_LOAD`).
  - Allocation et mapping de pages virtuelles aux adresses `p_vaddr`.
  - Copie des segments de code/données et mise à zéro de la section BSS.
  - Démarrage de l'exécution au point d'entrée `e_entry`.

---

## 6. Architecture Matérielle & Drivers (Phase 7)

### Énumération du Bus PCI (Phase 7a.1)
- Accès au *Configuration Space* PCI via ports I/O `0xCF8` (Address) et `0xCFC` (Data).
- Scan complet du Bus 0 (32 slots $\times$ 8 fonctions).
- Lecture des identifiants Vendor/Device, Classes, Subclasses, Prog IF, BARs et Header Type.
- Fonctions de recherche `pci_find_device()` et `pci_find_class()`.

### Parsing ACPI (Phase 7a.2)
- Récupération du pointeur RSDP via la structure de handoff UEFI.
- Validation du checksum RSDP (signature `"RSD PTR "`).
- Découverte et validation de la table XSDT (64-bit).
- Parsing des tables :
  - **MADT (Multiple APIC Description Table)** : Local APIC base, I/O APIC entries, Interrupt Source Overrides.
  - **FADT (Fixed ACPI Description Table)** : Ports PM, registres SMI/SCI.
  - **MCFG** : Base MMIO PCIe ECAM.

### Gestionnaires d'Interruptions LAPIC & I/O APIC (Phase 7a.3)
- Désactivation complète du PIC 8259 hérité (masquage de toutes les IRQ).
- Initialisation et activation du **Local APIC (LAPIC)** : Registre SVR, TPR = 0, envoi d'EOI APIC pour acquitter les interruptions.
- Initialisation de l'**I/O APIC** :
  - Configuration de la table de redirection (24 entrées).
  - Prise en compte des *Interrupt Source Overrides* ACPI (ex: IRQ0 Timer routé vers GSI 2).
  - Routage des ISA IRQs 0-15 vers les vecteurs 32-47.

### Routage des Interruptions PCI (Phase 7a.4)
- Lecture des registres PIRQ du chipset PIIX3 (`0x60-0x63`).
- Association des broches INT A/B/C/D vers les GSIs 16-19.
- Configuration des vecteurs IDT 48-51 (Level-triggered, Active-low).

### Contrôleur USB xHCI (Phase 7b)
- **Découverte (7b.1)** : Détection de la classe PCI `0x0C / 0x03 / 0x30`, lecture et mapping MMIO du BAR0 64-bit, lecture des Capability Registers (`CAPLENGTH`, `HCIVERSION`, `HCSPARAMS1-3`, `HCCPARAMS1`, `DBOFF`, `RTSOFF`).
- **Reset & Anneaux (7b.2)** :
  - Arrêt et Reset du contrôleur (`USBCMD.HCRST`).
  - Allocation et programmation du tableau DCBAA (`DCBAAP`).
  - Allocation du **Command Ring** (256 TRBs terminés par un Link TRB avec toggle cycle).
  - Allocation de l'**Event Ring** (256 TRBs) et de la table **ERST** (Event Ring Segment Table).
  - Configuration de l'Interrupter 0 (`IMAN`, `ERSTSZ`, `ERSTBA`, `ERDP`).
  - Démarrage du contrôleur (`USBCMD.RUN = 1`, `USBSTS.HCH == 0`).
- **Commandes & Ports (7b.3)** :
  - Moteur de soumission de commandes (`xhci_send_command`) avec sonnerie Doorbell 0.
  - Réception et acquittement des événements de complétion sur l'Event Ring via `ERDP`.
  - Commandes `NO_OP` et `ENABLE_SLOT`.
  - Détection de présence matérielle (`PORTSC.CCS`), réinitialisation de port (`PORTSC.PR`), lecture de la vitesse de négociation (USB Full/Low/High/SuperSpeed).
- **Énumération & Transferts de Contrôle (7b.4)** :
  - Allocation des contextes xHCI (Input Control, Slot, EP0) et Device Context dans `DCBAA[Slot_ID]`.
  - Exécution de la commande `ADDRESS_DEVICE` (TRB Type 11).
  - Émission de la requête standard `GET_DESCRIPTOR` via l'anneau de transfert EP0 (Setup Stage $\rightarrow$ Data Stage $\rightarrow$ Status Stage).
  - Parsing du descripteur de périphérique USB standard (Vendor ID, Product ID, Device Class, Number of Configurations).

### Contrôleur Stockage AHCI / SATA (Phase 7c)
- Découverte PCI de la classe `0x01 / 0x06 / 0x01` (ou contrôleur Intel ICH9).
- Mapping MMIO de l'ABAR (BAR5) et activation du mode AHCI (`GHC.AE = 1`).
- Énumération des 32 ports implémentés (`PI` bitmask).
- Détection de présence matérielle (`PxSSTS.DET == 3`) et lecture de signature (`AHCI_SIG_ATA` pour disques durs/SSD SATA, `AHCI_SIG_ATAPI` pour lecteurs optiques).
- Allocation et programmation des structures Command List (`PxCLB`) et Received FIS (`PxFB`), et démarrage du port (`PxCMD.ST | PxCMD.FRE`).

### Contrôleur Stockage NVMe PCIe (Phase 7d)
- Découverte PCI de la classe `0x01 / 0x08 / 0x02` (NVM Express).
- Mapping MMIO de BAR0 64-bit et lecture des capacités / version (`CAP`, `VS`).
- Allocation et programmation de l'Admin Submission Queue (`ASQ`, 64 entrées) et de l'Admin Completion Queue (`ACQ`, 64 entrées) via `AQA`.
- Activation du contrôleur (`CC.EN = 1`) avec vérification de l'état prêt (`CSTS.RDY = 1`).
- Envoi de la commande Admin `IDENTIFY Controller` (Opcode 0x06) avec sonnerie du Doorbell SQ 0 et lecture du modèle et numéro de série du SSD.

### Contrôleur Audio Intel High Definition Audio (HDA) (Phase 7e)
- Découverte PCI de la classe `0x04 / 0x03 / 0x00`.
- Mapping MMIO de BAR0 64-bit.
- Séquence de réinitialisation matérielle (`GCTL.CRST = 0` puis `GCTL.CRST = 1`).
- Détection des codecs connectés via `STATESTS`.
- Allocation et démarrage des anneaux CORB (Command Outbound Ring Buffer) et RIRB (Response Inbound Ring Buffer).

---

## 7. Sous-Système Réseau (Phase 8)

### Driver Ethernet Intel e1000 (82540EM / Gigabit)
- Découverte PCI du contrôleur réseau (Device ID `8086:100E` ou classe réseau).
- Mapping MMIO de BAR0 et configuration du lien (`CTRL.SLU = 1`).
- Lecture de l'adresse MAC matérielle via registres `RAL0`/`RAH0` ou EEPROM (`EERD`).
- Allocation de l'anneau de réception RX (32 descripteurs de buffers 2 Ko) et configuration de `RCTL`.
- Allocation de l'anneau d'émission TX (16 descripteurs de buffers 2 Ko) et configuration de `TCTL`.
- Moteur d'émission de trames Ethernet (`e1000_send_packet`) et de réception (`e1000_recv_packet`).

### Pile Réseau TCP/IP (Phase 8 — `net.c`)

**Structure de l'Interface Réseau** (`struct net_if`) :
- Adresse MAC (copiée depuis le driver e1000), IPv4 `10.0.2.15`, masque `255.255.255.0`, passerelle `10.0.2.2`, DNS `10.0.2.3` (réseau QEMU `-netdev user`).
- Compteurs de statistiques : `rx_packets`, `tx_packets`, `rx_bytes`, `tx_bytes`.

**Couche Liaison Ethernet II** :
- `eth_send(dst_mac, ethertype, payload, len)` : Construit une trame Ethernet II (header 14 octets + payload), padding à 60 octets minimum, puis appelle `e1000_send_packet`.
- `net_poll()` : Lit les paquets reçus avec `e1000_recv_packet` et les dispatche par EtherType (`0x0806` → ARP, `0x0800` → IPv4).

**Protocole ARP** :
- Cache ARP : Tableau de 16 entrées (`struct arp_entry { ip, mac[6], valid }`). Recherche O(n).
- `arp_resolve(ip, mac_out)` : Si IP en cache → retour immédiat. Sinon, envoi d'une requête ARP broadcast (*who-has*) puis polling de `net_poll()` jusqu'à réception de la réponse (ou timeout).
- `handle_arp(packet, len)` : Insère l'émetteur dans le cache. Si opération = REQUEST et `target_ip == our_ip`, envoie une réponse ARP unicast (*is-at*).

**Protocole IPv4** :
- `net_checksum(data, len)` : Somme en complément à 1 sur 16 bits (RFC 791).
- `ipv4_send(dst_ip, protocol, payload, len)` : Construit un en-tête IPv4 (version 4, IHL = 5, TTL = 64, Don't Fragment `0x4000`), calcule le checksum, détermine le prochain saut (même sous-réseau → direct, sinon → `g_net_if.gateway`), résout l'adresse MAC via `arp_resolve`, et envoie via `eth_send`.

**Protocole ICMP** :
- `icmp_ping(dst_ip, seq)` : Construit un paquet Echo Request (type 8, code 0) avec identifiant `0x1234` et 32 octets de payload. Calcule le checksum ICMP et appelle `ipv4_send` avec `IP_PROTO_ICMP`.
- `handle_ipv4 → ICMP` : Répondeur automatique : si le paquet reçu est un Echo Request destiné à notre IP, construit un Echo Reply (type 0), recalcule le checksum et envoie via `ipv4_send`.

**Protocole UDP** :
- `udp_send(dst_ip, src_port, dst_port, data, len)` : Construit un en-tête UDP (8 octets, checksum = 0 optionnel en IPv4) et envoie via `ipv4_send` avec `IP_PROTO_UDP`.

**Fonctions Utilitaires** :
- `htons / ntohs / htonl / ntohl` : Conversions Byte Order (Little Endian hôte ↔ Big Endian réseau).
- `print_ip(ip)` : Affichage d'une adresse IPv4 au format pointé sur la liaison série.


