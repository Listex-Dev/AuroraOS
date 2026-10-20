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
           kernel/fs/mount.c \
           kernel/fs/procfs.c \
           kernel/fs/devtmpfs.c \
           kernel/fs/tmpfs.c \
           kernel/fs/sysfs.c \
           kernel/fs/cpio.c \
           kernel/fs/m6_test.c \
           kernel/fs/m7_test.c \
           kernel/drivers/block.c \
           kernel/drivers/ahci.c \
           kernel/drivers/ata.c \
           kernel/drivers/ramdisk.c \
           kernel/fs/ext2.c \
           kernel/fs/ext2_mkfs.c \
           kernel/fs/ext2_fsck.c \
           kernel/fs/m8_test.c \
           kernel/drivers/console.c \
           kernel/drivers/tty.c \
           kernel/drivers/pty.c \
           kernel/drivers/kbd.c \
           kernel/drivers/pci.c \
           kernel/drivers/m9_test.c \
           kernel/proc/m10_test.c \
           kernel/proc/m11_test.c \
           kernel/drivers/m12_test.c \
           kernel/net/netbuf.c \
           kernel/net/netdev.c \
           kernel/net/loopback.c \
           kernel/net/ethernet.c \
           kernel/net/arp.c \
           kernel/net/ipv4.c \
           kernel/net/icmp.c \
           kernel/net/udp.c \
           kernel/net/tcp.c \
           kernel/net/socket.c \
           kernel/net/dhcp.c \
           kernel/drivers/net/e1000.c \
           kernel/net/m13_test.c \
           kernel/net/m14_test.c \
           kernel/proc/m15_test.c \
           kernel/proc/m16_test.c \
           kernel/fs/docs.c \
           kernel/proc/m17_test.c \
           kernel/proc/m18_test.c \
           kernel/module/module.c \
           kernel/proc/m19_test.c

ASM_SRCS := boot/boot.S \
            kernel/arch/x86_64/idt_asm.S \
            kernel/arch/x86_64/context.S \
            kernel/arch/x86_64/syscall_entry.S \
            kernel/proc/user_test.S \
            kernel/proc/user_m5.S \
            kernel/fs/user_vfs.S \
            kernel/fs/user_m7.S \
            kernel/fs/user_m8.S \
            kernel/drivers/user_m9.S

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS)) \
        $(patsubst %.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

TARGET_ELF := $(BUILD_DIR)/aurora.elf
TARGET_BIN := $(BUILD_DIR)/aurora.bin
DISK_IMG   := $(BUILD_DIR)/aurora.img
HELLO_ELF  := $(BUILD_DIR)/hello.elf
USER_BINS  := $(BUILD_DIR)/bin/ls \
              $(BUILD_DIR)/bin/cat \
              $(BUILD_DIR)/bin/cp \
              $(BUILD_DIR)/bin/ps \
              $(BUILD_DIR)/bin/vi \
              $(BUILD_DIR)/bin/grep \
              $(BUILD_DIR)/bin/test_m10 \
              $(BUILD_DIR)/bin/sh \
              $(BUILD_DIR)/bin/getty \
              $(BUILD_DIR)/bin/login \
              $(BUILD_DIR)/bin/init \
              $(BUILD_DIR)/bin/test_m11 \
              $(BUILD_DIR)/bin/mknod \
              $(BUILD_DIR)/bin/fdisk \
              $(BUILD_DIR)/bin/mount \
              $(BUILD_DIR)/bin/test_m12 \
              $(BUILD_DIR)/bin/ping \
              $(BUILD_DIR)/bin/test_m13 \
              $(BUILD_DIR)/bin/dhclient \
              $(BUILD_DIR)/bin/nslookup \
              $(BUILD_DIR)/bin/ifconfig \
              $(BUILD_DIR)/bin/netstat \
              $(BUILD_DIR)/bin/wget \
              $(BUILD_DIR)/bin/test_m14 \
              $(BUILD_DIR)/bin/id \
              $(BUILD_DIR)/bin/su \
              $(BUILD_DIR)/bin/chmod \
              $(BUILD_DIR)/bin/test_m15 \
              $(BUILD_DIR)/bin/syslogd \
              $(BUILD_DIR)/bin/logger \
              $(BUILD_DIR)/bin/crond \
              $(BUILD_DIR)/bin/crontab \
              $(BUILD_DIR)/bin/sshd \
              $(BUILD_DIR)/bin/telinit \
              $(BUILD_DIR)/bin/touch \
              $(BUILD_DIR)/bin/echo \
              $(BUILD_DIR)/bin/test_m16 \
              $(BUILD_DIR)/bin/pkg \
              $(BUILD_DIR)/bin/man \
              $(BUILD_DIR)/bin/test_m17 \
              $(BUILD_DIR)/bin/dmesg \
              $(BUILD_DIR)/bin/test_m18 \
              $(BUILD_DIR)/bin/insmod \
              $(BUILD_DIR)/bin/rmmod \
              $(BUILD_DIR)/bin/lsmod \
              $(BUILD_DIR)/bin/test_m19

.PHONY: all kernel run run-term debug test clean ssh

all: kernel

kernel: $(TARGET_ELF) $(TARGET_BIN) $(DISK_IMG)

$(BUILD_DIR)/rootfs.ext4: $(HELLO_ELF) $(USER_BINS) scripts/prepare_rootfs.sh
	@bash scripts/prepare_rootfs.sh

$(DISK_IMG): $(BUILD_DIR)/rootfs.ext4
	@mkdir -p $(dir $@)
	@dd if=/dev/zero of=$@ bs=1M count=64 status=none
	@echo -e 'label: dos\n,32768,83\n,,83\n' | sfdisk -q $@ >/dev/null 2>&1 || true
	@dd if=$(BUILD_DIR)/rootfs.ext4 of=$@ seek=17 bs=1M conv=notrunc status=none

$(HELLO_ELF): user/hello.S
	@mkdir -p $(dir $@)
	@echo "  CC      $< -> $@"
	@$(CC) -nostdlib -static -fno-pie -no-pie $< -o $@

$(BUILD_DIR)/libc/libc.a $(USER_BINS):
	@bash scripts/build_userland.sh

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

run: $(TARGET_BIN) $(DISK_IMG)
	@echo "Starting AuroraOS in QEMU GUI..."
	qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-netdev user,id=net0,hostfwd=tcp::2222-:22 -device e1000,netdev=net0 \
		-serial mon:stdio \
		-vga std \
		-no-reboot -no-shutdown

run-term: $(TARGET_BIN) $(DISK_IMG)
	@echo "Starting AuroraOS in terminal mode (press Ctrl-A X to exit QEMU)..."
	qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-netdev user,id=net0,hostfwd=tcp::2222-:22 -device e1000,netdev=net0 \
		-nographic \
		-serial mon:stdio \
		-no-reboot -no-shutdown

tools/aurora-ssh: tools/aurora-ssh.c
	@mkdir -p tools
	@gcc -O2 tools/aurora-ssh.c -o tools/aurora-ssh

ssh: tools/aurora-ssh
	@./tools/aurora-ssh

debug: $(TARGET_ELF) $(TARGET_BIN) $(DISK_IMG)
	@echo "Starting AuroraOS in debug mode (GDB port :1234)..."
	qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) \
		-drive file=$(DISK_IMG),format=raw,if=ide \
		-netdev user,id=net0 -device e1000,netdev=net0 \
		-serial stdio \
		-display none \
		-no-reboot -no-shutdown \
		-s -S

test: $(TARGET_BIN) $(DISK_IMG)
	@echo "Running Milestone automated acceptance test..."
	@output=$$(timeout 30s qemu-system-x86_64 \
		-machine q35 -m 512M -smp 1 \
		-kernel $(TARGET_BIN) -append "test" \
		-drive file=$(DISK_IMG),format=raw,if=ide,file.locking=off,snapshot=on \
		-netdev user,id=net0 -device e1000,netdev=net0 \
		-serial stdio \
		-display none \
		-no-reboot -no-shutdown 2>&1 || true); \
	echo "$$output"; \
	if echo "$$output" | grep -q "Milestone M19 status: \[OK\]"; then \
		echo ""; \
		echo ">>> TEST PASSED: Milestone M19 Acceptance Criteria Verified! <<<"; \
		exit 0; \
	else \
		echo ""; \
		echo ">>> TEST FAILED: Milestone M19 verification failed! <<<"; \
		exit 1; \
	fi

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Cleaned build directory."
