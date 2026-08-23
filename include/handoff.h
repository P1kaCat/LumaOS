/*
 * handoff.h — Structure de handoff bootloader → kernel
 *
 * Partagée entre le bootloader UEFI (PE32+, ABI Microsoft x64)
 * et le kernel LumaOS (ELF, ABI System V AMD64).
 *
 * Le bootloader alloue cette struct en mémoire (EFI_LOADER_DATA),
 * la remplit, puis passe un pointeur au kernel.
 */

#ifndef LUMAOS_HANDOFF_H
#define LUMAOS_HANDOFF_H

#include <stdint.h>

/* Magic : "LUMA" pour valider le handoff */
#define LUMAOS_HANDOFF_MAGIC 0x4C554D414F530000ULL

/* Formats de pixels UEFI */
#define LUMAOS_PIXEL_RGB  0
#define LUMAOS_PIXEL_BGR  1

struct lumaos_handoff {
    uint64_t magic;

    /* Framebuffer */
    uint64_t framebuffer;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_bpp;
    uint32_t fb_format;

    /* Memory map UEFI */
    uint64_t memory_map;
    uint64_t memory_map_size;
    uint64_t memory_map_desc_size;
    uint32_t memory_map_desc_version;

    uint32_t reserved;

    /* ACPI (Phase 7a.2) */
    uint64_t rsdp;  /* Physical address of RSDP (ACPI 2.0+), 0 = not found */
};

#endif /* LUMAOS_HANDOFF_H */
