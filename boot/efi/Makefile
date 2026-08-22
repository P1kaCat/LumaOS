# Makefile — LumaOS Phase 0A (Bootloader UEFI)
#
# Usage : cd boot/efi && make [build|run|clean]
#
# Prérequis :
#   - LLVM/Clang + LLD dans le PATH
#   - QEMU (qemu-system-x86_64) dans le PATH
#   - OVMF_CODE.fd et OVMF_VARS.fd dans ../../tools/ovmf/
#   - make + cp + rm (via MSYS2) dans le PATH

# --- Chemins (relatifs à boot/efi/) ---
BUILD_DIR  := ../../build
OVMF_DIR   := ../../tools/ovmf
EFI_ROOT   := $(BUILD_DIR)/efi_root

# --- Toolchain ---
CC         := clang
QEMU       := qemu-system-x86_64

# Flags compilation (C -> object file COFF)
EFI_CFLAGS  := --target=x86_64-unknown-windows -ffreestanding -fshort-wchar \
               -mno-red-zone -mno-stack-arg-probe -O2 -Wall -Wextra \
               -Wno-unused-parameter

# Flags linkage (object file -> PE32+ UEFI)
EFI_LDFLAGS := --target=x86_64-unknown-windows -nostdlib -fuse-ld=lld-link \
               -Wl,-entry:efi_main -Wl,-subsystem:efi_application

# --- Cibles ---
EFI_BIN     := $(BUILD_DIR)/boot/BOOTX64.EFI
EFI_OBJ     := $(BUILD_DIR)/boot/efi_main.o

.PHONY: all build run clean

all: build

# build : compile + link + prepare le dossier virtuel FAT pour QEMU
build: $(EFI_BIN)
	@mkdir -p $(EFI_ROOT)/EFI/BOOT
	@cp $(EFI_BIN) $(EFI_ROOT)/EFI/BOOT/BOOTX64.EFI
	@echo "Build complete: $(EFI_BIN)"
	@echo "EFI root ready: $(EFI_ROOT)/EFI/BOOT/BOOTX64.EFI"

$(EFI_BIN): efi_main.c efi_types.h
	@mkdir -p $(BUILD_DIR)/boot
	$(CC) $(EFI_CFLAGS) -c efi_main.c -o $(EFI_OBJ)
	$(CC) $(EFI_LDFLAGS) -o $@ $(EFI_OBJ)

# run : build + QEMU avec OVMF
run: build
	@cp $(OVMF_DIR)/OVMF_VARS.fd $(BUILD_DIR)/ovmf_vars.fd
	@echo "Launching QEMU + OVMF..."
	$(QEMU) \
	  -drive if=pflash,format=raw,unit=0,file=$(OVMF_DIR)/OVMF_CODE.fd,readonly=on \
	  -drive if=pflash,format=raw,unit=1,file=$(BUILD_DIR)/ovmf_vars.fd \
	  -drive file=fat:rw:$(EFI_ROOT),format=raw,media=disk \
	  -serial stdio

# clean : supprime le dossier de build
clean:
	@rm -rf $(BUILD_DIR)
	@echo "Clean: build/ removed"
