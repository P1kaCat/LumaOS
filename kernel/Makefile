# kernel/Makefile — LumaOS Kernel (Phase 0B)
#
# Usage : cd kernel && make [all|clean]
#
# Prérequis : LLVM/Clang + LLD dans le PATH
# Pas besoin de NASM — Clang assemble boot.S directement.

BUILD_DIR := ../build/kernel

# --- Toolchain ---
CC := clang
LD := ld.lld

# Include path pour handoff.h
INCLUDE := -I../include

# Flags compilation C (kernel bare-metal ELF)
KERNEL_CFLAGS := --target=x86_64-unknown-none -ffreestanding -nostdlib \
                 -O2 -Wall -Wextra -mno-red-zone -fno-pic -fno-stack-protector \
                 $(INCLUDE)

# Flags compilation assembly (boot.S via Clang intégré)
ASM_FLAGS := --target=x86_64-unknown-none -ffreestanding -nostdlib

# Flags linkage (ELF64 avec linker script custom)
KERNEL_LDFLAGS := -T linker.ld -nostdlib --oformat elf64-x86-64

# --- Cibles ---
KERNEL_BIN := $(BUILD_DIR)/kernel.elf
KERNEL_OBJ := $(BUILD_DIR)/kernel.o
BOOT_OBJ   := $(BUILD_DIR)/boot.o

.PHONY: all clean

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(BOOT_OBJ) $(KERNEL_OBJ) linker.ld
	@mkdir -p $(BUILD_DIR)
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(BOOT_OBJ) $(KERNEL_OBJ)
	@echo "Kernel built: $@"

$(KERNEL_OBJ): kernel.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c kernel.c -o $@

$(BOOT_OBJ): boot.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASM_FLAGS) -c boot.S -o $@

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Kernel build cleaned"
