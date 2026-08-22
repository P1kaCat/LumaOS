# kernel/Makefile — LumaOS Kernel (Phase 0B)
#
# Usage : cd kernel && make [all|clean]
#
# Prérequis : LLVM/Clang + NASM + LLD dans le PATH

BUILD_DIR := ../build/kernel

# --- Toolchain ---
CC   := clang
NASM := nasm
LD   := ld.lld

# Include path pour handoff.h
INCLUDE := -I../include

# Flags compilation C (kernel bare-metal ELF)
KERNEL_CFLAGS := --target=x86_64-unknown-none -ffreestanding -nostdlib \
                 -O2 -Wall -Wextra -mno-red-zone -fno-pic -fno-stack-protector \
                 $(INCLUDE)

# Flags NASM (ELF64)
NASM_FLAGS := -f elf64

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

$(BOOT_OBJ): boot.asm
	@mkdir -p $(BUILD_DIR)
	$(NASM) $(NASM_FLAGS) -o $@ boot.asm

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Kernel build cleaned"
