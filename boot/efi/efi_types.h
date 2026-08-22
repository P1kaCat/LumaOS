/*
 * efi_types.h — Types UEFI minimaux pour LumaOS
 *
 * Définit manuellement les structures UEFI nécessaires au bootloader.
 * Aucune dépendance externe (pas d'EDK2, pas de GNU-EFI).
 *
 * Basé sur la spec UEFI 2.x :
 *   §2.3.1 — Types de base (EFI_HANDLE, EFI_STATUS, CHAR16)
 *   §4.3   — EFI_SYSTEM_TABLE, EFI_TABLE_HEADER
 *   §12.4  — EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
 */

#ifndef LUMAOS_EFI_TYPES_H
#define LUMAOS_EFI_TYPES_H

#include <stddef.h>  /* wchar_t (disponible en mode freestanding) */

/* ===== Types de base ===== */

/* EFI_STATUS : code de retour des fonctions UEFI. UINTN = 64 bits en x86-64. */
typedef unsigned long long efi_status_t;

/* UINT32 */
typedef unsigned int efi_uint_t;

/*
 * CHAR16 : caractère UEFI (UCS-2, 2 octets).
 * wchar_t = 2 bytes avec --target=x86_64-unknown-windows + -fshort-wchar.
 * UEFI utilise des chaînes UCS-2 (pas UTF-8, pas UTF-16).
 */
typedef wchar_t efi_char16_t;

/* EFI_HANDLE : handle opaque passé par le firmware. C'est un void *. */
typedef void *efi_handle_t;

/* EFI_SUCCESS = 0 */
#define EFI_SUCCESS 0

/* ===== Structures UEFI ===== */

/*
 * EFI_TABLE_HEADER — header commun à toutes les tables UEFI (spec §4.3)
 *
 * 24 octets : signature(8) + revision(4) + header_size(4) + crc32(4) + reserved(4)
 */
struct efi_table_header {
    unsigned long long signature;
    unsigned int       revision;
    unsigned int       header_size;
    unsigned int       crc32;
    unsigned int       reserved;
};

/*
 * EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL — sortie texte UEFI (spec §12.4)
 *
 * 80 octets : 10 pointeurs (8 octets chacun).
 * On définit les prototypes complets pour output_string et clear_screen
 * (les deux fonctions utilisées en Phase 0A).
 * Les autres sont des void* pour préserver le layout de la structure.
 */
struct efi_simple_text_output_protocol {
    void         *reset;                  /* Reset(This, ExtendedVerification) */
    efi_status_t (*output_string)(        /* OutputString(This, String) */
        struct efi_simple_text_output_protocol *self,
        efi_char16_t *string);
    void         *test_string;             /* TestString(This, String) */
    void         *query_mode;              /* QueryMode(This, ModeNum, Cols, Rows) */
    void         *set_mode;                /* SetMode(This, ModeNum) */
    void         *set_attribute;           /* SetAttribute(This, Attr) */
    efi_status_t (*clear_screen)(          /* ClearScreen(This) */
        struct efi_simple_text_output_protocol *self);
    void         *set_cursor_position;     /* SetCursorPosition(This, Col, Row) */
    void         *enable_cursor;           /* EnableCursor(This, Enable) */
    void         *mode;                    /* *Mode */
};

/*
 * EFI_SYSTEM_TABLE — table système passée à efi_main (spec §4.3)
 *
 * 120 octets. Layout complet pour préserver les offsets.
 * Le firmware UEFI construit cette table en mémoire et passe un pointeur
 * vers elle dans RDX (2e argument, ABI Microsoft x64).
 *
 * Offset 64 : con_out — le seul champ utilisé en Phase 0A.
 */
struct efi_system_table {
    struct efi_table_header header;                    /* 0   (24 bytes) */
    efi_char16_t           *firmware_vendor;            /* 24  (8 bytes) */
    efi_uint_t              firmware_revision;          /* 32  (4 bytes + 4 pad) */
    efi_handle_t            console_in_handle;          /* 40  (8 bytes) */
    void                   *con_in;                     /* 48  (8 bytes) */
    efi_handle_t            console_out_handle;         /* 56  (8 bytes) */
    struct efi_simple_text_output_protocol *con_out;    /* 64  (8 bytes) ← utilisé */
    efi_handle_t            standard_error_handle;      /* 72  (8 bytes) */
    void                   *std_err;                    /* 80  (8 bytes) */
    void                   *runtime_services;           /* 88  (8 bytes) */
    void                   *boot_services;              /* 96  (8 bytes) */
    unsigned long long      number_of_table_entries;    /* 104 (8 bytes) */
    void                   *configuration_table;        /* 112 (8 bytes) */
};

#endif /* LUMAOS_EFI_TYPES_H */
