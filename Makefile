# Makefile — LumaOS Root Build
#
# Usage :
#   make build    — Compile le kernel + bootloader + prépare l'image
#   make run      — Build + lance QEMU avec OVMF + disk image
#   make debug    — Build + QEMU avec gdbstub (freeze au boot, port 1234)
#   make disk     — Create FAT32 disk image (Phase 6)
#   make clean    — Nettoie tout
#
# Prérequis :
#   - LLVM/Clang + LLD dans le PATH
#   - QEMU (qemu-system-x86_64) dans le PATH
#   - OVMF_CODE.fd et OVMF_VARS.fd dans tools/ovmf/
#   - Python 3 dans le PATH

# --- Chemins ---
BUILD_DIR := build
OVMF_DIR  := tools/ovmf
EFI_ROOT  := $(BUILD_DIR)/efi_root
DISK_IMG  := $(BUILD_DIR)/disk.img

# --- Toolchain ---
QEMU := qemu-system-x86_64
PYTHON := python3

# --- Binaires ---
EFI_BIN    := $(BUILD_DIR)/boot/BOOTX64.EFI
KERNEL_BIN := $(BUILD_DIR)/kernel/kernel.elf

.PHONY: all build run debug clean kernel bootloader image disk

all: build

# build : tout compiler + créer l'image FAT virtuelle + disk image
build: kernel bootloader image disk

kernel:
	@echo "=== Building Kernel ==="
	$(MAKE) -C kernel

bootloader:
	@echo "=== Building Bootloader ==="
	$(MAKE) -C boot/efi

# image : copier BOOTX64.EFI + kernel.elf dans le FAT root pour QEMU
image: kernel bootloader
	@mkdir -p $(EFI_ROOT)/EFI/BOOT
	@cp $(EFI_BIN) $(EFI_ROOT)/EFI/BOOT/BOOTX64.EFI
	@cp $(KERNEL_BIN) $(EFI_ROOT)/kernel.elf
	@echo "=== Image ready ==="
	@echo "  BOOTX64.EFI → $(EFI_ROOT)/EFI/BOOT/BOOTX64.EFI"
	@echo "  kernel.elf  → $(EFI_ROOT)/kernel.elf"

# disk : create FAT32 disk image for Phase 6 filesystem
disk:
	@echo "=== Creating FAT32 disk image ==="
	$(PYTHON) tools/create_disk.py $(DISK_IMG)

# run : build + QEMU avec OVMF + data disk
run: build
	@cp $(OVMF_DIR)/OVMF_VARS.fd $(BUILD_DIR)/ovmf_vars.fd
	@echo "=== Launching QEMU + OVMF ==="
	$(QEMU) \
	  -drive if=pflash,format=raw,unit=0,file=$(OVMF_DIR)/OVMF_CODE.fd,readonly=on \
	  -drive if=pflash,format=raw,unit=1,file=$(BUILD_DIR)/ovmf_vars.fd \
	  -drive file=fat:rw:$(EFI_ROOT),format=raw,media=disk \
	  -drive file=$(DISK_IMG),format=raw,if=ide,index=1 \
	  -serial stdio

# debug : build + QEMU freeze au boot (gdbstub, port 1234)
debug: build
	@cp $(OVMF_DIR)/OVMF_VARS.fd $(BUILD_DIR)/ovmf_vars.fd
	@echo "=== Launching QEMU (debug mode, waiting for GDB on port 1234) ==="
	$(QEMU) \
	  -drive if=pflash,format=raw,unit=0,file=$(OVMF_DIR)/OVMF_CODE.fd,readonly=on \
	  -drive if=pflash,format=raw,unit=1,file=$(BUILD_DIR)/ovmf_vars.fd \
	  -drive file=fat:rw:$(EFI_ROOT),format=raw,media=disk \
	  -drive file=$(DISK_IMG),format=raw,if=ide,index=1 \
	  -serial stdio \
	  -s -S

# clean : tout nettoyer
clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C boot/efi clean
	@rm -rf $(BUILD_DIR)
	@echo "=== Clean: build/ removed ==="
