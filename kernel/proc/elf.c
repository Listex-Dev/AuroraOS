#include <aurora/elf64.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/errno.h>
#include <aurora/arch/x86_64/cr.h>

int elf64_load_pml4(uint64_t *pml4, const void *elf_data, size_t elf_size, uint64_t *entry_out, uint64_t *brk_out)
{
    if (!elf_data || elf_size < sizeof(Elf64_Ehdr)) {
        return -ENOEXEC;
    }

    const Elf64_Ehdr *hdr = (const Elf64_Ehdr *)elf_data;

    /* Validate ELF magic */
    if (hdr->e_ident[EI_MAG0] != ELFMAG0 ||
        hdr->e_ident[EI_MAG1] != ELFMAG1 ||
        hdr->e_ident[EI_MAG2] != ELFMAG2 ||
        hdr->e_ident[EI_MAG3] != ELFMAG3) {
        printk("<3>[ELF] Invalid ELF magic header\n");
        return -ENOEXEC;
    }

    /* Validate 64-bit architecture */
    if (hdr->e_ident[EI_CLASS] != ELFCLASS64) {
        printk("<3>[ELF] Not a 64-bit ELF binary\n");
        return -ENOEXEC;
    }

    /* Validate Little-Endian format */
    if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        printk("<3>[ELF] Not little-endian\n");
        return -ENOEXEC;
    }

    /* Validate target machine x86-64 */
    if (hdr->e_machine != EM_X86_64) {
        printk("<3>[ELF] Target machine is not x86-64 (got %d)\n", hdr->e_machine);
        return -ENOEXEC;
    }

    /* Validate executable type */
    if (hdr->e_type != ET_EXEC && hdr->e_type != ET_DYN) {
        printk("<3>[ELF] Not an executable ELF (type %d)\n", hdr->e_type);
        return -ENOEXEC;
    }

    if (!pml4) {
        pml4 = vmm_get_kernel_pml4();
    }

    /* Temporarily activate pml4 in CR3 if needed so user addresses are directly accessible */
    uint64_t orig_cr3 = read_cr3();
    uint64_t target_cr3 = vmm_virt_to_phys_direct(pml4);
    if (orig_cr3 != target_cr3) {
        write_cr3(target_cr3);
    }

    uint64_t max_vaddr = 0;
    const uint8_t *ph_table = (const uint8_t *)elf_data + hdr->e_phoff;

    for (uint16_t i = 0; i < hdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)(ph_table + (i * hdr->e_phentsize));

        if (phdr->p_type == PT_LOAD) {
            if (phdr->p_memsz == 0) {
                continue;
            }

            uint64_t seg_start = PAGE_ALIGN_DOWN(phdr->p_vaddr);
            uint64_t seg_end   = PAGE_ALIGN(phdr->p_vaddr + phdr->p_memsz);

            /* Determine page permissions */
            uint64_t pte_flags = PTE_USER | PTE_WRITABLE;
            if (!(phdr->p_flags & PF_X)) {
                pte_flags |= PTE_NX;
            }

            /* Map required virtual pages */
            for (uint64_t v = seg_start; v < seg_end; v += PAGE_SIZE) {
                uint64_t existing_phys = vmm_virt_to_phys(pml4, v);
                if (!existing_phys) {
                    uintptr_t phys = pmm_alloc_page();
                    if (!phys) {
                        if (orig_cr3 != target_cr3) write_cr3(orig_cr3);
                        return -ENOMEM;
                    }
                    memset(vmm_phys_to_virt(phys), 0, PAGE_SIZE);
                    vmm_map_page(pml4, v, phys, pte_flags);
                }
            }

            /* Copy segment data from file */
            if (phdr->p_filesz > 0) {
                memcpy((void *)phdr->p_vaddr,
                       (const uint8_t *)elf_data + phdr->p_offset,
                       phdr->p_filesz);
            }

            /* Zero out remaining BSS area */
            if (phdr->p_memsz > phdr->p_filesz) {
                memset((void *)(phdr->p_vaddr + phdr->p_filesz),
                       0,
                       phdr->p_memsz - phdr->p_filesz);
            }

            if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
                max_vaddr = phdr->p_vaddr + phdr->p_memsz;
            }
        }
    }

    /* Restore CR3 if caller was using a different page directory */
    if (orig_cr3 != target_cr3) {
        write_cr3(orig_cr3);
    }

    if (entry_out) {
        *entry_out = hdr->e_entry;
    }
    if (brk_out) {
        *brk_out = PAGE_ALIGN(max_vaddr);
    }

    printk("<6>[ELF] Successfully loaded ELF64 executable: entry=%p, brk=%p\n",
           (void *)hdr->e_entry, (void *)PAGE_ALIGN(max_vaddr));

    return 0;
}

int elf64_load(const void *elf_data, size_t elf_size, uint64_t *entry_out, uint64_t *brk_out)
{
    return elf64_load_pml4(vmm_get_kernel_pml4(), elf_data, elf_size, entry_out, brk_out);
}
