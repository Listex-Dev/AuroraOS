ENTRY(_start)

OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

KERNEL_PHYS_BASE = 0x100000;
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS
{
    /* Physical address start: 1 MiB */
    . = KERNEL_PHYS_BASE;

    /* Multiboot headers must be within first 8 KiB */
    .multiboot ALIGN(8) : {
        KEEP(*(.multiboot))
    }

    .boot.text ALIGN(4K) : {
        *(.boot.text)
    }

    .boot.data ALIGN(4K) : {
        *(.boot.data)
    }

    .boot.bss ALIGN(4K) : {
        *(.boot.bss)
    }

    /* Move location counter to Higher-Half virtual address */
    . += KERNEL_VIRT_BASE;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRT_BASE) {
        *(.text .text.*)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRT_BASE) {
        *(.data .data.*)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRT_BASE) {
        *(COMMON)
        *(.bss .bss.*)
    }

    kernel_end = .;
    kernel_end_phys = . - KERNEL_VIRT_BASE;

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}
