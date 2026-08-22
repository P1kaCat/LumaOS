/*
 * efi_main.c — Bootloader UEFI LumaOS (Phase 0A)
 *
 * Objectif : afficher "LumaOS" à l'écran via les services UEFI, puis attendre.
 *
 * Le firmware UEFI charge BOOTX64.EFI en mémoire et appelle efi_main().
 * L'ABI est Microsoft x64 : RCX = image_handle, RDX = system_table.
 * Le target Clang --target=x86_64-unknown-windows gère l'ABI automatiquement.
 */

#include "efi_types.h"

/*
 * efi_main — point d'entrée du bootloader
 *
 * @param image_handle  Handle du bootloader (non utilisé en Phase 0A)
 * @param st            Pointeur vers la EFI_SYSTEM_TABLE
 * @return              EFI_STATUS (jamais atteint — boucle infinie)
 */
efi_status_t efi_main(efi_handle_t image_handle, struct efi_system_table *st) {
    /* 1. Effacer l'écran via ConOut->ClearScreen */
    st->con_out->clear_screen(st->con_out);

    /* 2. Afficher "LumaOS" via ConOut->OutputString */
    efi_char16_t msg[] = L"LumaOS\r\n";
    st->con_out->output_string(st->con_out, msg);

    /* 3. Rester à l'écran */
    for (;;) {}

    /* Inatteignable — satisfait le type de retour */
    return EFI_SUCCESS;
}
