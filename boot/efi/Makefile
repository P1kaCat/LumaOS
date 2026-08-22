# boot/efi/Makefile — Bootloader UEFI LumaOS (Phase 0A + 0B)
#
# Usage : cd boot/efi && make [all|clean]
# Le Makefile root gère le build complet + QEMU.

BUILD_DIR := ../../build

# --- Toolchain ---
CC := clang

# Include path pour handoff.h (shared avec kernel)
INCLUDE := -I../../include

# Flags compilation (C → object file COFF)
EFI_CFLAGS := --target=x86_64-unknown-windows -ffreestanding -fshort-wchar \
              -mno-red-zone -mno-stack-arg-probe -O2 -Wall -Wextra \
              -Wno-unused-parameter $(INCLUDE)

# Flags linkage (object file → PE32+ UEFI)
EFI_LDFLAGS := --target=x86_64-unknown-windows -nostdlib -fuse-ld=lld-link \
               -Wl,-entry:efi_main -Wl,-subsystem:efi_application

# --- Cibles ---
EFI_BIN := $(BUILD_DIR)/boot/BOOTX64.EFI
EFI_OBJ := $(BUILD_DIR)/boot/efi_main.o

.PHONY: all clean

all: $(EFI_BIN)

$(EFI_BIN): efi_main.c efi_types.h elf.h
	@mkdir -p $(BUILD_DIR)/boot
	$(CC) $(EFI_CFLAGS) -c efi_main.c -o $(EFI_OBJ)
	$(CC) $(EFI_LDFLAGS) -o $@ $(EFI_OBJ)
	@echo "Bootloader built: $@"

clean:
	@rm -rf $(BUILD_DIR)/boot
	@echo "Bootloader build cleaned"
