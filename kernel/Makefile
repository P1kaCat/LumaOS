# kernel/Makefile — LumaOS Kernel (Phase 0C)
# Prérequis : LLVM/Clang + LLD. Pas besoin de NASM.

BUILD_DIR := ../build/kernel
CC := clang
LD := ld.lld
INCLUDE := -I../include

KERNEL_CFLAGS := --target=x86_64-unknown-none -ffreestanding -nostdlib \
                 -O2 -Wall -Wextra -mno-red-zone -fno-pic -fno-stack-protector \
                 $(INCLUDE)

ASM_FLAGS := --target=x86_64-unknown-none -ffreestanding -nostdlib

KERNEL_LDFLAGS := -T linker.ld -nostdlib --oformat elf64-x86-64

KERNEL_BIN := $(BUILD_DIR)/kernel.elf

.PHONY: all clean

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/cpu.o $(BUILD_DIR)/isr.o $(BUILD_DIR)/mem.o linker.ld
	@mkdir -p $(BUILD_DIR)
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(BUILD_DIR)/boot.o $(BUILD_DIR)/cpu.o $(BUILD_DIR)/isr.o $(BUILD_DIR)/mem.o $(BUILD_DIR)/kernel.o
	@echo "Kernel built: $@"

$(BUILD_DIR)/kernel.o: kernel.c cpu.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c kernel.c -o $@

$(BUILD_DIR)/cpu.o: cpu.c cpu.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c cpu.c -o $@

$(BUILD_DIR)/isr.o: isr.S cpu.h
$(BUILD_DIR)/mem.o: mem.c mem.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c mem.c -o $@
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASM_FLAGS) -c isr.S -o $@

$(BUILD_DIR)/boot.o: boot.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASM_FLAGS) -c boot.S -o $@

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Kernel build cleaned"
