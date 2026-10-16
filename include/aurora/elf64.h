#ifndef _AURORA_ELF64_H
#define _AURORA_ELF64_H

#include <aurora/types.h>

#define EI_NIDENT 16

/* ELF Identification Indexes */
#define EI_MAG0       0
#define EI_MAG1       1
#define EI_MAG2       2
#define EI_MAG3       3
#define EI_CLASS      4
#define EI_DATA       5
#define EI_VERSION    6
#define EI_OSABI      7
#define EI_ABIVERSION 8

#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1

#define ET_EXEC 2
#define ET_DYN  3

#define EM_X86_64 62

/* Segment Types */
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7
#define PT_GNU_STACK 0x6474E551

/* Segment Flags */
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

/* Auxiliary Vector Types (System V AMD64 ABI) */
#define AT_NULL   0
#define AT_IGNORE 1
#define AT_EXECFD 2
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_BASE   7
#define AT_FLAGS  8
#define AT_ENTRY  9
#define AT_NOTELF 10
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14
#define AT_RANDOM 25

typedef struct {
    uint8_t  e_ident[EI_NIDENT];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

typedef struct {
    uint64_t a_type;
    uint64_t a_val;
} Elf64_auxv_t;

int elf64_load(const void *elf_data, size_t elf_size, uint64_t *entry_out, uint64_t *brk_out);
int elf64_load_pml4(uint64_t *pml4, const void *elf_data, size_t elf_size, uint64_t *entry_out, uint64_t *brk_out);

#endif /* _AURORA_ELF64_H */
