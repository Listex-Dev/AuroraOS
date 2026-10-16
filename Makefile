# AuroraOS Makefile
# Targets: kernel, run, debug, test, clean

CC      := gcc
LD      := ld
AS      := gcc
OBJCOPY := objcopy

CFLAGS  := -Wall -Wextra -Werror -O2 -fno-omit-frame-pointer \
           -ffreestanding -fno-stack-protector -fno-pie -no-pie \
           -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
           -nostdlib -Iinclude

ASFLAGS := -c -Iinclude

LDFLAGS := -nostdlib -z max-page-size=0x1000 -T boot/linker.ld

BUILD_DIR := build

C_SRCS  := kernel/main.c \
           kernel/panic.c \
           kernel/printk.c \
           kernel/bootinfo.c \
           kernel/lib/string.c \
           kernel/drivers/serial.c \
           kernel/arch/x86_64/tss.c \
           kernel/arch/x86_64/gdt.c \
           kernel/arch/x86_64/idt.c \
           kernel/arch/x86_64/exceptions.c \
           kernel/arch/x86_64/m1_test.c \
           kernel/mm/pmm.c \
           kernel/mm/vmm.c \
           kernel/mm/heap.c \
           kernel/mm/m2_test.c \
           kernel/arch/x86_64/pic.c \
           kernel/arch/x86_64/pit.c \
           kernel/proc/task.c \
           kernel/proc/sched.c \
           kernel/proc/m3_test.c \
           kernel/syscall/syscall.c \
           kernel/syscall/sys_file.c \
           kernel/syscall/sys_proc.c \
           kernel/syscall/sys_mm.c \
           kernel/proc/m4_test.c \
           kernel/proc/elf.c \
           kernel/proc/stack.c \
           kernel/proc/binaries.c \
           kernel/proc/m5_test.c \
           kernel/fs/vfs.c \
           kernel/fs/dentry.c \
           kernel/fs/inode.c \
           kernel/fs/namei.c \
           kernel/fs/file.c \
           kernel/fs/ramfs.c \
           kernel/fs/m6_test.c

ASM_SRCS := boot/boot.S \
            kernel/arch/x86_64/idt_asm.S \
            kernel/arch/x86_64/context.S \
            kernel/arch/x86_64/syscall_entry.S \
            kernel/proc/user_test.S \
            kernel/proc/user_m5.S \
            kernel/proc/builtin_binaries.S \
            kernel/fs/user_vfs.S

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

TARGET_ELF := $(BUILD_DIR)/aurora.elf
TARGET_BIN := $(BUILD_DIR)/aurora.bin
HELLO_ELF  := $(BUILD_DIR)/hello.elf

.PHONY: all kernel run debug test clean

all: kernel

kernel: $(TARGET_ELF) $(TARGET_BIN)

$(HELLO_ELF): user/hello.S
	@mkdir -p $(dir $@)
	@echo "  CC      $< -> $@"
	@$(CC) -nostdlib -static -fno-pie -no-pie $< -o $@

$(BUILD_DIR)/kernel/proc/builtin_binaries.o: $(HELLO_ELF)

$(TARGET_ELF): $(OBJS) boot/linker.ld
	@mkdir -p $(dir $@)
	@echo "  LD      $@"
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET_BIN): $(TARGET_ELF)
	@echo "  OBJCOPY $@ (Multiboot bootable image)"
	@$(OBJCOPY) -O elf32-i386 $< $@
	@echo "Kernel build complete: $(TARGET_ELF) and $(TARGET_BIN)"

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	@echo "  AS      $<"
	@$(AS) $(ASFLAGS) $< -o $@

run: $(TARGET_BIN)
	@echo "Starting AuroraOS in QEMU (press Ctrl-A then X to exit)..."
	qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) \
		-serial stdio \
		-display none \
		-no-reboot -no-shutdown

debug: $(TARGET_ELF) $(TARGET_BIN)
	@echo "Starting AuroraOS in debug mode (GDB port :1234)..."
	qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) \
		-serial stdio \
		-display none \
		-no-reboot -no-shutdown \
		-s -S

test: $(TARGET_BIN)
	@echo "Running Milestone automated acceptance test..."
	@output=$$(timeout 6s qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) \
		-serial stdio \
		-display none \
		-no-reboot -no-shutdown 2>&1 || true); \
	echo "$$output"; \
	if echo "$$output" | grep -q "Milestone M6 status: \[OK\]"; then \
		echo ""; \
		echo ">>> TEST PASSED: Milestone M6 Acceptance Criteria Verified! <<<"; \
		exit 0; \
	else \
		echo ""; \
		echo ">>> TEST FAILED: Milestone M6 verification failed! <<<"; \
		exit 1; \
	fi

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory."
