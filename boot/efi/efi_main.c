/*
 * efi_main.c — Bootloader UEFI LumaOS (Phase 0A + 0B + 7a.2)
 *
 * Phase 0A : afficher "LumaOS" à l'écran
 * Phase 0B : charger kernel.elf depuis le disque, récupérer le framebuffer
 *           et le memory map, exit boot services, puis sauter au kernel.
 * Phase 7a.2 : chercher le RSDP ACPI 2.0+ dans les EFI configuration tables
 *              et passer le pointeur au kernel via le handoff struct.
 *
 * Flow :
 *   1. Récupérer GOP (framebuffer)
 *   2. Ouvrir le filesystem, lire kernel.elf
 *   3. Parser ELF, charger les segments à 0x100000
 *   4. Récupérer le memory map
 *   5. Chercher le RSDP ACPI 2.0+ dans les configuration tables
 *   6. Construire le handoff struct (avec rsdp)
 *   7. ExitBootServices
 *   8. Appeler le kernel avec le handoff struct en argument
 *
 * L'ABI est Microsoft x64 : RCX = image_handle, RDX = system_table.
 */

#include "efi_types.h"
#include "elf.h"
#include "handoff.h"

/* ===== Utilitaires ===== */

/* Mémoire zero-fill */
static void zero_fill(void *dst, unsigned long long n) {
    unsigned char *p = (unsigned char *)dst;
    for (unsigned long long i = 0; i < n; i++) p[i] = 0;
}

/* memcpy simple */
static void copy_mem(void *dst, const void *src, unsigned long long n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (unsigned long long i = 0; i < n; i++) d[i] = s[i];
}

/* Afficher une chaîne UCS-2 via ConOut */
static void efi_print(struct efi_system_table *st, const efi_char16_t *s) {
    st->con_out->output_string(st->con_out, (efi_char16_t *)s);
}

/* Afficher un message d'erreur et halt */
static void efi_error(struct efi_system_table *st, const efi_char16_t *msg) {
    efi_print(st, L"\r\n[ERROR] ");
    efi_print(st, msg);
    efi_print(st, L"\r\n");
    for (;;) {}
}

/* Comparer deux GUIDs (16 bytes) */
static int guid_equal(const struct efi_guid *a, const struct efi_guid *b) {
    unsigned char *pa = (unsigned char *)a;
    unsigned char *pb = (unsigned char *)b;
    for (int i = 0; i < 16; i++)
        if (pa[i] != pb[i]) return 0;
    return 1;
}

/* ===== Buffer statique pour le memory map ===== */
static unsigned char mmap_buffer[16384];  /* 16 KB — largement suffisant pour QEMU */

/* ===== Bootloader principal ===== */

efi_status_t efi_main(efi_handle_t image_handle, struct efi_system_table *st) {
    struct efi_boot_services *bs = st->boot_services;

    /* --- 0. Écran d'accueil --- */
    st->con_out->clear_screen(st->con_out);
    efi_print(st, L"LumaOS — Bootloader Phase 0B\r\n");

    /* --- 1. Récupérer le framebuffer (GOP) --- */
    efi_print(st, L"[*] Locating GOP...\r\n");

    struct efi_gop *gop = NULL;
    efi_status_t s = bs->locate_protocol(
        (struct efi_guid *)&EFI_GOP_GUID, NULL, (void **)&gop);
    if (s != EFI_SUCCESS || !gop)
        efi_error(st, L"GOP not found");
    if (!gop->mode || !gop->mode->info)
        efi_error(st, L"GOP mode info missing");

    struct efi_gop_mode_info *gop_info = gop->mode->info;
    efi_print(st, L"[+] GOP: framebuffer ready\r\n");

    /* --- 2. Ouvrir le filesystem et lire kernel.elf --- */
    efi_print(st, L"[*] Opening filesystem...\r\n");

    struct efi_simple_file_system_protocol *sfs = NULL;
    s = bs->locate_protocol(
        (struct efi_guid *)&EFI_SFS_GUID, NULL, (void **)&sfs);
    if (s != EFI_SUCCESS || !sfs)
        efi_error(st, L"SimpleFileSystem not found");

    struct efi_file_protocol *root = NULL;
    s = sfs->open_volume(sfs, &root);
    if (s != EFI_SUCCESS || !root)
        efi_error(st, L"OpenVolume failed");

    struct efi_file_protocol *kernel_file = NULL;
    s = root->open(root, &kernel_file, L"kernel.elf", EFI_FILE_MODE_READ, 0);
    if (s != EFI_SUCCESS || !kernel_file)
        efi_error(st, L"kernel.elf not found on disk");

    /* Allouer un buffer pour lire le kernel (2 MB — largement suffisant) */
    unsigned char *kernel_buf = NULL;
    s = bs->allocate_pool(EFI_LOADER_DATA, 2 * 1024 * 1024, (void **)&kernel_buf);
    if (s != EFI_SUCCESS || !kernel_buf)
        efi_error(st, L"AllocatePool failed for kernel buffer");

    unsigned long long kernel_size = 2 * 1024 * 1024;
    s = kernel_file->read(kernel_file, &kernel_size, kernel_buf);
    kernel_file->close(kernel_file);

    if (s != EFI_SUCCESS || kernel_size == 0)
        efi_error(st, L"Failed to read kernel.elf");

    efi_print(st, L"[+] kernel.elf loaded (");
    /* kernel_size contient la taille réelle lue */

    /* --- 3. Parser l'ELF et charger les segments --- */
    efi_print(st, L"[*] Parsing ELF...\r\n");

    struct elf64_ehdr *ehdr = (struct elf64_ehdr *)kernel_buf;

    /* Valider le magic ELF */
    if (ehdr->e_ident[0] != ELFMAG0 ||
        ehdr->e_ident[1] != ELFMAG1 ||
        ehdr->e_ident[2] != ELFMAG2 ||
        ehdr->e_ident[3] != ELFMAG3)
        efi_error(st, L"Not an ELF file");

    if (ehdr->e_ident[4] != ELFCLASS64)
        efi_error(st, L"Not ELF64");

    if (ehdr->e_machine != EM_X86_64)
        efi_error(st, L"Not x86-64 ELF");

    uint64_t entry_point = ehdr->e_entry;

    /* Allouer la mémoire pour le kernel à l'adresse physique du linker (0x100000) */
    /* On calcule la plage d'adresses nécessaire */
    uint64_t load_addr = 0xFFFFFFFFFFFFFFFFULL;
    uint64_t end_addr  = 0;

    struct elf64_phdr *phdrs = (struct elf64_phdr *)(kernel_buf + ehdr->e_phoff);
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;
        if (phdrs[i].p_paddr < load_addr)
            load_addr = phdrs[i].p_paddr;
        uint64_t seg_end = phdrs[i].p_paddr + phdrs[i].p_memsz;
        if (seg_end > end_addr)
            end_addr = seg_end;
    }

    /* Allouer les pages pour le kernel */
    uint64_t kernel_pages = (end_addr - load_addr + 4095) / 4096;
    uint64_t alloc_addr = load_addr;
    s = bs->allocate_pages(EFI_ALLOCATE_ADDRESS, EFI_LOADER_DATA,
                           kernel_pages, &alloc_addr);
    if (s != EFI_SUCCESS || alloc_addr != load_addr)
        efi_error(st, L"AllocatePages at 0x100000 failed");

    /* Charger chaque segment PT_LOAD */
    for (int i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uint8_t *dest = (uint8_t *)phdrs[i].p_paddr;
        uint8_t *src  = kernel_buf + phdrs[i].p_offset;

        /* Copier les données du fichier */
        copy_mem(dest, src, phdrs[i].p_filesz);

        /* Zero-fill le bss (memsz - filesz) */
        if (phdrs[i].p_memsz > phdrs[i].p_filesz)
            zero_fill(dest + phdrs[i].p_filesz,
                      phdrs[i].p_memsz - phdrs[i].p_filesz);
    }

    efi_print(st, L"[+] Kernel loaded at 0x100000\r\n");

    /* --- 4. Récupérer le memory map --- */
    efi_print(st, L"[*] Getting memory map...\r\n");

    unsigned long long mmap_size = sizeof(mmap_buffer);
    unsigned long long map_key = 0;
    unsigned long long desc_size = 0;
    unsigned int desc_version = 0;

    s = bs->get_memory_map(&mmap_size, mmap_buffer,
                           &map_key, &desc_size, &desc_version);
    if (s != EFI_SUCCESS)
        efi_error(st, L"GetMemoryMap failed");

    /* --- 5. Chercher le RSDP ACPI 2.0+ dans les configuration tables --- */
    efi_print(st, L"[*] Searching ACPI tables...\r\n");

    uint64_t rsdp_addr = 0;
    {
        struct efi_configuration_table *ct =
            (struct efi_configuration_table *)st->configuration_table;
        unsigned long long n = st->number_of_table_entries;

        for (unsigned long long i = 0; i < n; i++) {
            if (guid_equal(&ct[i].vendor_guid,
                           (struct efi_guid *)&EFI_ACPI_20_GUID)) {
                rsdp_addr = (uint64_t)(uintptr_t)ct[i].vendor_table;
                break;
            }
        }
    }

    if (rsdp_addr)
        efi_print(st, L"[+] ACPI 2.0+ RSDP found\r\n");
    else
        efi_print(st, L"[!] ACPI 2.0+ RSDP not found\r\n");

    /* --- 6. Construire le handoff struct --- */
    /* Alloué en EFI_LOADER_DATA — persiste après ExitBootServices */
    struct lumaos_handoff *ho = NULL;
    s = bs->allocate_pool(EFI_LOADER_DATA, sizeof(struct lumaos_handoff),
                          (void **)&ho);
    if (s != EFI_SUCCESS || !ho)
        efi_error(st, L"AllocatePool failed for handoff");

    zero_fill(ho, sizeof(struct lumaos_handoff));
    ho->magic = LUMAOS_HANDOFF_MAGIC;

    /* Framebuffer */
    ho->framebuffer    = gop->mode->frame_buffer_base;
    ho->fb_width       = gop_info->horizontal_resolution;
    ho->fb_height      = gop_info->vertical_resolution;
    ho->fb_pitch       = gop_info->pixels_per_scan_line * 4;  /* 32 bpp */
    ho->fb_bpp         = 32;
    ho->fb_format      = gop_info->pixel_format;

    /* Memory map */
    ho->memory_map            = (uint64_t)(uintptr_t)mmap_buffer;
    ho->memory_map_size       = mmap_size;
    ho->memory_map_desc_size  = desc_size;
    ho->memory_map_desc_version = desc_version;

    /* ACPI RSDP pointer (Phase 7a.2) */
    ho->rsdp = rsdp_addr;

    /* --- 7. ExitBootServices (avec retry) --- */
    efi_print(st, L"[*] Exiting boot services...\r\n");

    s = bs->exit_boot_services(image_handle, map_key);
    if (s != EFI_SUCCESS) {
        /* Retry : le memory map a pu changer */
        mmap_size = sizeof(mmap_buffer);
        bs->get_memory_map(&mmap_size, mmap_buffer,
                           &map_key, &desc_size, &desc_version);
        s = bs->exit_boot_services(image_handle, map_key);
        if (s != EFI_SUCCESS) {
            /* À ce stade on ne peut plus utiliser ConOut, mais on essaie */
            st->con_out->output_string(st->con_out, L"ExitBootServices failed\r\n");
            for (;;) {}
        }
    }

    /* --- 8. Sauter au kernel --- */
    /* Le kernel est en ELF (System V ABI). Le 1er argument va dans RDI. */
    /* On place le pointeur handoff dans RDI et on saute au point d'entrée. */

    void (*kernel_entry)(void) = (void(*)(void))entry_point;

    __asm__ volatile (
        "mov %0, %%rdi\n\t"   /* handoff pointer → RDI (System V ABI arg1) */
        "jmp *%1\n\t"         /* saut au kernel entry point */
        :
        : "r"((uint64_t)(uintptr_t)ho),
          "r"((uint64_t)(uintptr_t)kernel_entry)
        : "rdi"
    );

    /* Inatteignable */
    for (;;) {}
    return EFI_SUCCESS;
}
