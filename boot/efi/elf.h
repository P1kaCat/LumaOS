/*
 * elf.h — Types ELF64 minimaux pour le bootloader
 *
 * Le bootloader parse le kernel.elf (format ELF64) pour :
 * - Valider le magic number
 * - Trouver le point d'entrée (e_entry)
 * - Charger les segments PT_LOAD en mémoire
 */

#ifndef LUMAOS_ELF_H
#define LUMAOS_ELF_H

#include <stdint.h>

/* ELF magic : 0x7F 'E' 'L' 'F' */
#define ELFMAG0  0x7f
#define ELFMAG1  'E'
#define ELFMAG2  'L'
#define ELFMAG3  'F'

/* ELF class */
#define ELFCLASS64  2

/* ELF machine */
#define EM_X86_64   62

/* Program header type */
#define PT_LOAD     1

/* ELF64 header (64 bytes) */
struct elf64_ehdr {
    uint8_t  e_ident[16];   /* 0  : magic + class + data + version + osabi */
    uint16_t e_type;         /* 16 : ET_EXEC=2, ET_DYN=3 */
    uint16_t e_machine;      /* 18 : EM_X86_64=62 */
    uint32_t e_version;      /* 20 */
    uint64_t e_entry;        /* 24 : point d'entrée (adresse virtuelle) */
    uint64_t e_phoff;        /* 32 : offset des program headers */
    uint64_t e_shoff;        /* 40 : offset des section headers */
    uint32_t e_flags;        /* 48 */
    uint16_t e_ehsize;       /* 52 : taille du header (64) */
    uint16_t e_phentsize;    /* 54 : taille d'un program header (56) */
    uint16_t e_phnum;       /* 56 : nombre de program headers */
    uint16_t e_shentsize;    /* 58 */
    uint16_t e_shnum;        /* 60 */
    uint16_t e_shstrndx;     /* 62 */
};

/* ELF64 program header (56 bytes) */
struct elf64_phdr {
    uint32_t p_type;     /* 0  : PT_LOAD=1 */
    uint32_t p_flags;    /* 4  : permissions (R=4, W=2, X=1) */
    uint64_t p_offset;   /* 8  : offset dans le fichier */
    uint64_t p_vaddr;    /* 16 : adresse virtuelle cible */
    uint64_t p_paddr;    /* 24 : adresse physique cible (no paging → = vaddr) */
    uint64_t p_filesz;   /* 32 : taille dans le fichier */
    uint64_t p_memsz;    /* 40 : taille en mémoire (si > filesz → bss zero-fill) */
    uint64_t p_align;    /* 48 : alignement */
};

#endif /* LUMAOS_ELF_H */
