# kernel/Makefile - LumaOS Kernel (Phase 6)
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

KERNEL_OBJS := $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/cpu.o \
               $(BUILD_DIR)/isr.o $(BUILD_DIR)/mem.o $(BUILD_DIR)/sched.o \
               $(BUILD_DIR)/user.o $(BUILD_DIR)/user_code.o $(BUILD_DIR)/ata.o

.PHONY: all clean

all: $(KERNEL_BIN)

$(KERNEL_BIN): $(KERNEL_OBJS) linker.ld
	@mkdir -p $(BUILD_DIR)
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(KERNEL_OBJS)
	@echo "Kernel built: $@"

$(BUILD_DIR)/kernel.o: kernel.c cpu.h mem.h sched.h user.h ata.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c kernel.c -o $@

$(BUILD_DIR)/cpu.o: cpu.c cpu.h sched.h mem.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c cpu.c -o $@

$(BUILD_DIR)/isr.o: isr.S cpu.h sched.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASM_FLAGS) -c isr.S -o $@

$(BUILD_DIR)/mem.o: mem.c mem.h ../include/handoff.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c mem.c -o $@

$(BUILD_DIR)/sched.o: sched.c sched.h cpu.h mem.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c sched.c -o $@

$(BUILD_DIR)/user.o: user.c user.h cpu.h sched.h mem.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c user.c -o $@

$(BUILD_DIR)/user_code.o: user_code.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASM_FLAGS) -c user_code.S -o $@

$(BUILD_DIR)/boot.o: boot.S
	@mkdir -p $(BUILD_DIR)
	$(CC) $(ASM_FLAGS) -c boot.S -o $@

$(BUILD_DIR)/ata.o: ata.c ata.h cpu.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(KERNEL_CFLAGS) -c ata.c -o $@

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Kernel build cleaned"
