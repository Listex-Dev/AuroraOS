/*
 * AuroraOS Bootloader Trampoline (x86-64)
 * Supports Multiboot 1 and Multiboot 2 specifications.
 * Sets up early page tables, enters 64-bit Long Mode, and jumps to higher-half kernel.
 */

.set MULTIBOOT1_MAGIC,    0x1BADB002
.set MULTIBOOT1_FLAGS,    0x00000003   /* ALIGN | MEMINFO */
.set MULTIBOOT1_CHECKSUM, -(MULTIBOOT1_MAGIC + MULTIBOOT1_FLAGS)

.set MULTIBOOT2_MAGIC,    0xE85250D6
.set MULTIBOOT2_ARCH,     0            /* i386 32-bit protected mode */

.section .multiboot, "a"
.align 8
/* --- Multiboot 1 Header --- */
.long MULTIBOOT1_MAGIC
.long MULTIBOOT1_FLAGS
.long MULTIBOOT1_CHECKSUM

/* --- Multiboot 2 Header --- */
.align 8
multiboot2_header_start:
.long MULTIBOOT2_MAGIC
.long MULTIBOOT2_ARCH
.long (multiboot2_header_end - multiboot2_header_start)
.long -(MULTIBOOT2_MAGIC + MULTIBOOT2_ARCH + (multiboot2_header_end - multiboot2_header_start))

/* End tag */
.short 0
.short 0
.long 8
multiboot2_header_end:

/* --- 32-bit Boot Trampoline --- */
.section .boot.text, "ax"
.code32
.global _start
_start:
    cli
    cld

    /* Save Multiboot magic (EAX) and MBI pointer (EBX) */
    mov %eax, boot_magic
    mov %ebx, boot_mbi

    /* 1. Point PML4[0] (identity map 0..512GB) to early_pdpt */
    mov $early_pdpt, %eax
    or $0x3, %eax                /* Present | Writable */
    mov %eax, early_pml4

    /* 2. Point PML4[256] (direct physical map 0xFFFF800000000000) to early_pdpt */
    mov %eax, early_pml4 + (256 * 8)

    /* 3. Point PML4[511] (higher half kernel 0xFFFFFFFF80000000) to early_pdpt_high */
    mov $early_pdpt_high, %eax
    or $0x3, %eax
    mov %eax, early_pml4 + (511 * 8)

    /* 4. In early_pdpt, entry 0 -> early_pd (0..1GB) */
    mov $early_pd, %eax
    or $0x3, %eax
    mov %eax, early_pdpt

    /* 5. In early_pdpt_high, entry 510 (-2GB) -> early_pd (0..1GB) */
    mov %eax, early_pdpt_high + (510 * 8)

    /* 6. Populate early_pd: 512 entries of 2MB huge pages = 1GB physical memory */
    mov $0, %ecx
    mov $0x83, %eax              /* Present | Writable | Huge Page (2MB) */
1:
    mov %eax, early_pd(,%ecx,8)
    movl $0, early_pd + 4(,%ecx,8)
    add $0x200000, %eax          /* +2 MiB */
    inc %ecx
    cmp $512, %ecx
    jne 1b

    /* 7. Enable PAE (Physical Address Extension) in CR4 (bit 5) */
    mov %cr4, %eax
    or $(1 << 5), %eax
    mov %eax, %cr4

    /* 8. Enable Long Mode in EFER MSR (0xC0000080, bit 8) */
    mov $0xC0000080, %ecx
    rdmsr
    or $(1 << 8), %eax
    wrmsr

    /* 9. Load CR3 with physical address of early_pml4 */
    mov $early_pml4, %eax
    mov %eax, %cr3

    /* 10. Enable Paging (PG bit 31) and Protection (PE bit 0) in CR0 */
    mov %cr0, %eax
    or $0x80000001, %eax
    mov %eax, %cr0

    /* 11. Load 64-bit GDT */
    lgdt (gdt64_ptr)

    /* 12. Far jump to 64-bit code */
    ljmp $0x08, $long_mode_entry

.code64
long_mode_entry:
    /* Reload data segment registers */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Indirect jump to higher-half kernel entry */
    mov $kernel_entry64, %rax
    jmp *%rax

.section .text
kernel_entry64:
    /* Set up 64-bit kernel stack */
    mov $kernel_stack_top, %rsp

    /* Pass bootloader magic and mbi pointer as first two arguments (SysV AMD64 ABI: RDI, RSI) */
    mov (boot_magic), %edi
    mov (boot_mbi), %esi

    /* Call C kernel entry */
    call kmain

    /* Safety halt loop if kmain returns */
halt_loop:
    cli
    hlt
    jmp halt_loop

/* --- Data and Tables --- */
.section .boot.data, "aw"
.align 16
gdt64:
    .quad 0x0000000000000000       /* Null descriptor */
    .quad 0x00209A0000000000       /* Kernel Code: 64-bit, DPL 0, Exec/Read (selector 0x08) */
    .quad 0x0000920000000000       /* Kernel Data: 64-bit, DPL 0, Read/Write (selector 0x10) */
gdt64_end:

gdt64_ptr:
    .short gdt64_end - gdt64 - 1   /* Limit */
    .long gdt64                    /* 32-bit linear address */

.global boot_magic
.global boot_mbi
boot_magic:
    .long 0
boot_mbi:
    .long 0

.section .boot.bss, "aw", @nobits
.align 4096
early_pml4:
    .skip 4096
early_pdpt:
    .skip 4096
early_pdpt_high:
    .skip 4096
early_pd:
    .skip 4096

.section .bss
.align 16
kernel_stack_bottom:
    .skip 16384                    /* 16 KiB early kernel stack */
kernel_stack_top:
