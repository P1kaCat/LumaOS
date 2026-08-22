/*
 * efi_types.h — Types UEFI pour LumaOS
 *
 * Définit manuellement les structures UEFI nécessaires.
 * Aucune dépendance externe (pas d'EDK2, pas de GNU-EFI).
 *
 * Basé sur la spec UEFI 2.x :
 *   §2.3.1 — Types de base
 *   §4.3   — EFI_SYSTEM_TABLE
 *   §7.2   — EFI_BOOT_SERVICES
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

/* ===== EFI_TABLE_HEADER ===== */

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

/* ===== Boot Services (§7.2) ===== */

struct efi_boot_services {
    void *raise_tpl;                                  /* 0   */
    void *restore_tpl;                                /* 8   */
    efi_status_t (*allocate_pages)(                   /* 16  */
        unsigned int allocate_type,
        unsigned int memory_type,
        unsigned long long pages,
        unsigned long long *memory);
    void *free_pages;                                  /* 24  */
    efi_status_t (*get_memory_map)(                    /* 32  */
        unsigned long long *memory_map_size,
        void *memory_map,
        unsigned long long *map_key,
        unsigned long long *descriptor_size,
        unsigned int *descriptor_version);
    efi_status_t (*allocate_pool)(                    /* 40  */
        unsigned int pool_type,
        unsigned long long size,
        void **buffer);
    void *free_pool;                                   /* 48  */
    void *create_event;                                /* 56  */
    void *set_timer;                                   /* 64  */
    void *wait_for_event;                              /* 72  */
    void *signal_event;                                /* 80  */
    void *close_event;                                  /* 88  */
    void *check_event;                                 /* 96  */
    void *install_protocol_interface;                   /* 104 */
    void *reinstall_protocol_interface;                /* 112 */
    void *uninstall_protocol_interface;                /* 120 */
    void *handle_protocol;                             /* 128 */
    void *reserved;                                    /* 136 */
    void *register_protocol_notify;                    /* 144 */
    void *locate_handle;                               /* 152 */
    void *locate_device_path;                           /* 160 */
    void *install_configuration_table;                  /* 168 */
    void *load_image;                                  /* 176 */
    void *start_image;                                 /* 184 */
    void *exit;                                        /* 192 */
    void *unload_image;                                 /* 200 */
    efi_status_t (*exit_boot_services)(                /* 208 */
        efi_handle_t image_handle,
        unsigned long long map_key);
    void *get_next_monotonic_count;                     /* 216 */
    void *stall;                                       /* 224 */
    void *set_watchdog_timer;                           /* 232 */
    void *connect_controller;                           /* 240 */
    void *disconnect_controller;                        /* 248 */
    void *open_protocol;                               /* 256 */
    void *close_protocol;                               /* 264 */
    void *open_protocol_information;                    /* 272 */
    void *protocols_per_handle;                        /* 280 */
    void *locate_handle_buffer;                         /* 288 */
    efi_status_t (*locate_protocol)(                    /* 296 */
        struct efi_guid *guid,
        void *registration,
        void **protocol_interface);
    void *install_multiple_protocol_interfaces;         /* 304 */
    void *uninstall_multiple_protocol_interfaces;       /* 312 */
    void *calculate_crc32;                             /* 320 */
    void *copy_mem;                                    /* 328 */
    void *set_mem;                                     /* 336 */
    void *create_event_ex;                             /* 344 */
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
