/*
 * efi_types.h — Types UEFI pour LumaOS
 *
 * Définit manuellement les structures UEFI nécessaires.
 * Aucune dépendance externe (pas d'EDK2, pas de GNU-EFI).
 *
 * Basé sur la spec UEFI 2.x :
 *   §2.3.1 — Types de base
 *   §4.3   — EFI_SYSTEM_TABLE
 *   §7.2   — EFI_BOOT_SERVICES (commence par EFI_TABLE_HEADER !)
 *   §12.4  — EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
 *   §13.4  — EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
 *   §13.5  — EFI_FILE_PROTOCOL
 *   §23.4  — EFI_GRAPHICS_OUTPUT_PROTOCOL
 */

#ifndef LUMAOS_EFI_TYPES_H
#define LUMAOS_EFI_TYPES_H

#include <stddef.h>

/* ===== Types de base ===== */

typedef unsigned long long efi_status_t;
typedef unsigned int       efi_uint_t;
typedef wchar_t            efi_char16_t;
typedef void              *efi_handle_t;

#define EFI_SUCCESS 0

/* Codes d'erreur UEFI fréquents */
#define EFI_NOT_FOUND   14
#define EFI_END_OF_FILE  36

/* Types de mémoire UEFI */
#define EFI_LOADER_DATA 2

/* Types d'allocation */
#define EFI_ALLOCATE_ADDRESS 2

/* ===== EFI_GUID ===== */

struct efi_guid {
    unsigned int  data1;
    unsigned short data2;
    unsigned short data3;
    unsigned char  data4[8];
};

/* GUID GOP : 9042A9DE-23DC-4A38-96FB-7ADED080516A */
static const struct efi_guid EFI_GOP_GUID = {
    0x9042a9de, 0x23dc, 0x4a38,
    { 0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a }
};

/* GUID SimpleFileSystem : 964E5B22-6459-11D2-8E39-00A0C969723B */
static const struct efi_guid EFI_SFS_GUID = {
    0x964e5b22, 0x6459, 0x11d2,
    { 0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b }
};

/* GUID ACPI 2.0+ : 8868E871-E4F1-11D3-BC22-0080C73C8881 */
static const struct efi_guid EFI_ACPI_20_GUID = {
    0x8868e871, 0xe4f1, 0x11d3,
    { 0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81 }
};

/* ===== EFI_TABLE_HEADER (24 bytes) ===== */

struct efi_table_header {
    unsigned long long signature;
    unsigned int       revision;
    unsigned int       header_size;
    unsigned int       crc32;
    unsigned int       reserved;
};

/* ===== Output texte (§12.4) ===== */

struct efi_simple_text_output_protocol {
    void         *reset;
    efi_status_t (*output_string)(
        struct efi_simple_text_output_protocol *self,
        efi_char16_t *string);
    void         *test_string;
    void         *query_mode;
    void         *set_mode;
    void         *set_attribute;
    efi_status_t (*clear_screen)(
        struct efi_simple_text_output_protocol *self);
    void         *set_cursor_position;
    void         *enable_cursor;
    void         *mode;
};

/* ===== Graphics Output Protocol (§23.4) ===== */

struct efi_pixel_bitmask {
    unsigned int red_mask;
    unsigned int green_mask;
    unsigned int blue_mask;
    unsigned int reserved_mask;
};

struct efi_gop_mode_info {
    unsigned int version;                 /* 0  */
    unsigned int horizontal_resolution;    /* 4  */
    unsigned int vertical_resolution;      /* 8  */
    unsigned int pixel_format;             /* 12 : 0=RGB 1=BGR 2=BitMask 3=BltOnly */
    struct efi_pixel_bitmask pixel_info;  /* 16 : 16 bytes */
    unsigned int pixels_per_scan_line;     /* 32 */
};  /* 36 bytes */

struct efi_gop_mode {
    unsigned int max_mode;                  /* 0  */
    unsigned int mode;                      /* 4  */
    struct efi_gop_mode_info *info;         /* 8  */
    unsigned long long size_of_info;        /* 16 */
    unsigned long long frame_buffer_base;   /* 24 */
    unsigned long long frame_buffer_size;   /* 32 */
};  /* 40 bytes */

struct efi_gop {
    void *query_mode;                       /* 0  */
    void *set_mode;                          /* 8  */
    void *blt;                               /* 16 */
    struct efi_gop_mode *mode;              /* 24 */
};

/* ===== Simple File System Protocol (§13.4) ===== */

struct efi_file_protocol;  /* forward */

struct efi_simple_file_system_protocol {
    unsigned long long revision;            /* 0  */
    efi_status_t (*open_volume)(            /* 8  */
        struct efi_simple_file_system_protocol *self,
        struct efi_file_protocol **root);
};

/* ===== File Protocol (§13.5) ===== */

struct efi_file_protocol {
    unsigned long long revision;                                /* 0   */
    efi_status_t (*open)(                                        /* 8   */
        struct efi_file_protocol *self,
        struct efi_file_protocol **new_handle,
        efi_char16_t *file_name,
        unsigned long long open_mode,
        unsigned long long attributes);
    efi_status_t (*close)(struct efi_file_protocol *self);       /* 16  */
    void         *delete_;                                       /* 24  */
    efi_status_t (*read)(                                        /* 32  */
        struct efi_file_protocol *self,
        unsigned long long *buffer_size,
        void *buffer);
    void         *write;                                         /* 40  */
    void         *get_position;                                   /* 48  */
    void         *set_position;                                  /* 56  */
    void         *get_info;                                      /* 64  */
    void         *set_info;                                      /* 72  */
    void         *flush;                                         /* 80  */
};

/* Modes d'ouverture de fichier */
#define EFI_FILE_MODE_READ 1

/*
 * ===== Boot Services (§7.2) =====
 *
 * IMPORTANT : EFI_BOOT_SERVICES commence par EFI_TABLE_HEADER (24 bytes).
 * Sans ce header, tous les offsets sont décalés de 24 bytes et on appelle
 * les mauvaises fonctions !
 */
struct efi_boot_services {
    struct efi_table_header header;                  /* 0   (24 bytes) */

    /* Task Priority Services */
    void *raise_tpl;                                  /* 24  */
    void *restore_tpl;                                /* 32  */

    /* Memory Services */
    efi_status_t (*allocate_pages)(                   /* 40  */
        unsigned int allocate_type,
        unsigned int memory_type,
        unsigned long long pages,
        unsigned long long *memory);
    void *free_pages;                                  /* 48  */
    efi_status_t (*get_memory_map)(                    /* 56  */
        unsigned long long *memory_map_size,
        void *memory_map,
        unsigned long long *map_key,
        unsigned long long *descriptor_size,
        unsigned int *descriptor_version);
    efi_status_t (*allocate_pool)(                    /* 64  */
        unsigned int pool_type,
        unsigned long long size,
        void **buffer);
    void *free_pool;                                   /* 72  */

    /* Event & Timer Services */
    void *create_event;                                /* 80  */
    void *set_timer;                                   /* 88  */
    void *wait_for_event;                              /* 96  */
    void *signal_event;                                /* 104 */
    void *close_event;                                  /* 112 */
    void *check_event;                                 /* 120 */

    /* Protocol Handler Services */
    void *install_protocol_interface;                   /* 128 */
    void *reinstall_protocol_interface;                /* 136 */
    void *uninstall_protocol_interface;                /* 144 */
    void *handle_protocol;                             /* 152 */
    void *reserved;                                    /* 160 */
    void *register_protocol_notify;                    /* 168 */
    void *locate_handle;                               /* 176 */
    void *locate_device_path;                           /* 184 */
    void *install_configuration_table;                  /* 192 */

    /* Image Services */
    void *load_image;                                  /* 200 */
    void *start_image;                                 /* 208 */
    void *exit;                                        /* 216 */
    void *unload_image;                                 /* 224 */
    efi_status_t (*exit_boot_services)(                /* 232 */
        efi_handle_t image_handle,
        unsigned long long map_key);

    /* Miscellaneous Services */
    void *get_next_monotonic_count;                     /* 240 */
    void *stall;                                       /* 248 */
    void *set_watchdog_timer;                           /* 256 */

    /* Driver Support Services */
    void *connect_controller;                           /* 264 */
    void *disconnect_controller;                        /* 272 */

    /* Open and Close Protocol Services */
    void *open_protocol;                               /* 280 */
    void *close_protocol;                               /* 288 */
    void *open_protocol_information;                    /* 296 */

    /* Library Services */
    void *protocols_per_handle;                        /* 304 */
    void *locate_handle_buffer;                         /* 312 */
    efi_status_t (*locate_protocol)(                    /* 320 */
        struct efi_guid *guid,
        void *registration,
        void **protocol_interface);
    void *install_multiple_protocol_interfaces;         /* 328 */
    void *uninstall_multiple_protocol_interfaces;       /* 336 */

    /* CRC Services */
    void *calculate_crc32;                             /* 344 */

    /* Miscellaneous Services */
    void *copy_mem;                                    /* 352 */
    void *set_mem;                                     /* 360 */
    void *create_event_ex;                             /* 368 */
};

/* ===== EFI_SYSTEM_TABLE (§4.3) ===== */

struct efi_system_table {
    struct efi_table_header header;                    /* 0   */
    efi_char16_t           *firmware_vendor;            /* 24  */
    efi_uint_t              firmware_revision;          /* 32  */
    efi_handle_t            console_in_handle;          /* 40  */
    void                   *con_in;                     /* 48  */
    efi_handle_t            console_out_handle;         /* 56  */
    struct efi_simple_text_output_protocol *con_out;    /* 64  */
    efi_handle_t            standard_error_handle;      /* 72  */
    void                   *std_err;                    /* 80  */
    void                   *runtime_services;           /* 88  */
    struct efi_boot_services *boot_services;            /* 96  */
    unsigned long long      number_of_table_entries;    /* 104 */
    void                   *configuration_table;        /* 112 */
};

/* ===== Configuration Table (§4.6) ===== */

struct efi_configuration_table {
    struct efi_guid vendor_guid;
    void           *vendor_table;
};  /* 24 bytes */

/* ===== Memory Descriptor (§7.4) ===== */

struct efi_memory_descriptor {
    unsigned int       type;                 /* 0  */
    unsigned int       _pad;                 /* 4  (alignement 64-bit) */
    unsigned long long physical_start;       /* 8  */
    unsigned long long virtual_start;         /* 16 */
    unsigned long long number_of_pages;      /* 24 */
    unsigned long long attribute;             /* 32 */
};  /* 40 bytes (le descriptor_size UEFI peut être plus grand) */

#endif /* LUMAOS_EFI_TYPES_H */
