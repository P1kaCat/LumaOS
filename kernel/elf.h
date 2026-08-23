/* elf.h — Minimal ELF64 structures for LumaOS program loader
 *
 * Used by spawn_file() to parse ELF64 executables loaded from FAT32.
 * Only the fields needed for PT_LOAD segment loading are included.
 */
#ifndef LUMAOS_ELF_H
#define LUMAOS_ELF_H

#include <stdint.h>

/* ===== ELF identification ===== */
#define EI_MAG0     0       /* e_ident[] index for magic byte 0 */
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define ELFMAG0     0x7F
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'
#define EI_CLASS    4
#define ELFCLASS64  2
#define EI_DATA     5
#define ELFDATA2LSB 1       /* little-endian */

/* ===== ELF types ===== */
#define ET_EXEC     2       /* executable file */
#define ET_DYN      3       /* shared object (PIE) — not supported */

/* ===== Machine types ===== */
#define EM_X86_64   62

/* ===== Program header types ===== */
#define PT_NULL     0
#define PT_LOAD     1       /* loadable segment */

/* ===== Program header flags ===== */
#define PF_X        0x1
#define PF_W        0x2
#define PF_R        0x4

/* ===== ELF64 Header (64 bytes, packed) ===== */
struct elf64_ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;       /* virtual entry point */
    uint64_t e_phoff;       /* program header table file offset */
    uint64_t e_shoff;       /* section header table file offset */
    uint32_t e_flags;
    uint16_t e_ehsize;      /* 64 */
    uint16_t e_phentsize;   /* 56 */
    uint16_t e_phnum;       /* number of program headers */
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed));

/* ===== ELF64 Program Header (56 bytes, packed) ===== */
struct elf64_phdr {
    uint32_t p_type;        /* PT_LOAD, etc. */
    uint32_t p_flags;       /* PF_R | PF_W | PF_X */
    uint64_t p_offset;      /* file offset of segment data */
    uint64_t p_vaddr;       /* virtual address to map at */
    uint64_t p_paddr;       /* physical address (unused, = p_vaddr) */
    uint64_t p_filesz;      /* size of segment in file */
    uint64_t p_memsz;       /* size of segment in memory (>= p_filesz) */
    uint64_t p_align;       /* alignment (usually PAGE_SIZE) */
} __attribute__((packed));

#endif /* LUMAOS_ELF_H */
