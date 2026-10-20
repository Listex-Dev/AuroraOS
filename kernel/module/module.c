/*
 * AuroraOS Loadable Kernel Module (LKM) Subsystem
 *
 * Implements dynamic ELF64 relocatable (.ko) module loading, symbol resolution,
 * relocation application, module lifecycle (init_module / cleanup_module),
 * and /proc/modules reporting.
 */

#include <aurora/module.h>
#include <aurora/elf64.h>
#include <aurora/string.h>
#include <aurora/printk.h>
#include <aurora/serial.h>
#include <aurora/mm/heap.h>
#include <aurora/errno.h>
#include <aurora/fs/vfs.h>
#include <aurora/drivers/block.h>
#include <aurora/drivers/pci.h>
#include <aurora/syscall.h>
#include <aurora/fs/mount.h>
#include <aurora/io.h>

#include <aurora/mm/vmm.h>
#include <aurora/mm/pmm.h>

#define MODULE_ADDR_START 0xFFFFFFFF88000000ULL
#define MODULE_ADDR_END   0xFFFFFFFF90000000ULL
#define MODULE_MAX_PAGES  ((MODULE_ADDR_END - MODULE_ADDR_START) / PAGE_SIZE)

static uint8_t g_module_bitmap[MODULE_MAX_PAGES / 8];

static inline bool mod_bitmap_test(size_t bit)
{
    return (g_module_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

static inline void mod_bitmap_set(size_t bit)
{
    g_module_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void mod_bitmap_clear(size_t bit)
{
    g_module_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

void *module_alloc(size_t size)
{
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    if (pages == 0 || pages > MODULE_MAX_PAGES) return NULL;

    size_t consecutive = 0;
    size_t start_bit = 0;
    bool found = false;

    for (size_t i = 0; i < MODULE_MAX_PAGES; i++) {
        if (!mod_bitmap_test(i)) {
            if (consecutive == 0) {
                start_bit = i;
            }
            consecutive++;
            if (consecutive == pages) {
                found = true;
                break;
            }
        } else {
            consecutive = 0;
        }
    }

    if (!found) {
        return NULL;
    }

    for (size_t i = 0; i < pages; i++) {
        mod_bitmap_set(start_bit + i);
    }

    uint64_t vaddr = MODULE_ADDR_START + start_bit * PAGE_SIZE;
    uint64_t *pml4 = vmm_get_kernel_pml4();

    for (size_t i = 0; i < pages; i++) {
        uintptr_t phys = pmm_alloc_page();
        if (!phys) {
            /* Cleanup already allocated and mapped pages */
            for (size_t j = 0; j < i; j++) {
                uint64_t cur = vaddr + j * PAGE_SIZE;
                uint64_t p = vmm_virt_to_phys(pml4, cur);
                vmm_unmap_page(pml4, cur);
                if (p) {
                    pmm_free_page(p & PAGE_MASK);
                }
            }
            for (size_t j = 0; j < pages; j++) {
                mod_bitmap_clear(start_bit + j);
            }
            return NULL;
        }
        memset(vmm_phys_to_virt(phys), 0, PAGE_SIZE);
        vmm_map_page(pml4, vaddr + i * PAGE_SIZE, phys, PTE_WRITABLE);
    }
    return (void *)vaddr;
}

void module_free(void *ptr, size_t size)
{
    if (!ptr || size == 0) return;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t vaddr = (uint64_t)(uintptr_t)ptr;
    if (vaddr < MODULE_ADDR_START || vaddr >= MODULE_ADDR_END) return;

    size_t start_bit = (vaddr - MODULE_ADDR_START) / PAGE_SIZE;
    uint64_t *pml4 = vmm_get_kernel_pml4();

    for (size_t i = 0; i < pages; i++) {
        uint64_t cur = vaddr + i * PAGE_SIZE;
        uint64_t phys = vmm_virt_to_phys(pml4, cur);
        vmm_unmap_page(pml4, cur);
        if (phys) {
            pmm_free_page(phys & PAGE_MASK);
        }
        if (start_bit + i < MODULE_MAX_PAGES) {
            mod_bitmap_clear(start_bit + i);
        }
    }
}

static struct module *g_module_list = NULL;

/* Kernel Exported Symbol Table */
static const struct kernel_symbol g_kernel_symbols[] = {
    /* Console & Diagnostics */
    { "printk",                      (void *)printk },
    { "serial_puts",                 (void *)serial_puts },

    /* Memory Management */
    { "kmalloc",                     (void *)kmalloc },
    { "kfree",                       (void *)kfree },
    { "kzalloc",                     (void *)kzalloc },

    /* String & Memory Ops */
    { "memcpy",                      (void *)memcpy },
    { "memset",                      (void *)memset },
    { "memmove",                     (void *)memmove },
    { "memcmp",                      (void *)memcmp },
    { "strlen",                      (void *)strlen },
    { "strcmp",                      (void *)strcmp },
    { "strncmp",                     (void *)strncmp },
    { "strcpy",                      (void *)strcpy },
    { "strncpy",                     (void *)strncpy },
    { "snprintf",                    (void *)snprintf },
    { "strstr",                      (void *)strstr },

    /* Port I/O */
    { "inb",                         (void *)inb },
    { "outb",                        (void *)outb },
    { "inw",                         (void *)inw },
    { "outw",                        (void *)outw },
    { "inl",                         (void *)inl },
    { "outl",                        (void *)outl },
    { "io_wait",                     (void *)io_wait },

    /* Block Layer & VFS */
    { "bdev_register",               (void *)bdev_register },
    { "bdev_unregister",             (void *)bdev_unregister },
    { "bdev_get",                    (void *)bdev_get },
    { "bdev_read",                   (void *)bdev_read },
    { "bdev_write",                  (void *)bdev_write },
    { "vfs_mount",                   (void *)vfs_mount },
    { "sys_open",                    (void *)sys_open },
    { "sys_read",                    (void *)sys_read },
    { "sys_write",                   (void *)sys_write },
    { "sys_close",                   (void *)sys_close },

    /* PCI Bus */
    { "pci_read_config8",            (void *)pci_read_config8 },
    { "pci_read_config16",           (void *)pci_read_config16 },
    { "pci_read_config32",           (void *)pci_read_config32 },
    { "pci_write_config8",           (void *)pci_write_config8 },
    { "pci_write_config16",          (void *)pci_write_config16 },
    { "pci_write_config32",          (void *)pci_write_config32 },
    { "pci_find_device",             (void *)pci_find_device },
    { "pci_find_class",              (void *)pci_find_class },
    { "pci_get_device_list",         (void *)pci_get_device_list },

    /* End marker */
    { NULL,                          NULL }
};

void *kallsyms_lookup_name(const char *name)
{
    if (!name) return NULL;

    /* 1. Search in exported kernel symbol table */
    for (int i = 0; g_kernel_symbols[i].name != NULL; i++) {
        if (strcmp(g_kernel_symbols[i].name, name) == 0) {
            return g_kernel_symbols[i].addr;
        }
    }

    /* 2. Search in exported symbols of loaded modules */
    for (struct module *m = g_module_list; m != NULL; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            return m->core_base;
        }
    }

    return NULL;
}

struct module *module_find(const char *name)
{
    if (!name) return NULL;
    for (struct module *m = g_module_list; m != NULL; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

struct module *module_get_list(void)
{
    return g_module_list;
}

size_t module_format_proc_modules(char *buf, size_t max_len)
{
    if (!buf || max_len == 0) return 0;
    size_t offset = 0;
    buf[0] = '\0';

    for (struct module *m = g_module_list; m != NULL; m = m->next) {
        const char *state_str = (m->state == MODULE_STATE_LIVE) ? "Live" :
                                (m->state == MODULE_STATE_COMING) ? "Loading" : "Unloading";
        int len = snprintf(buf + offset, max_len - offset,
                           "%s %lu %d %s 0x%llx\n",
                           m->name, (unsigned long)m->core_size, m->refcnt,
                           state_str, (unsigned long long)(uintptr_t)m->core_base);
        if (len > 0) {
            offset += (size_t)len;
        }
        if (offset >= max_len - 64) break;
    }

    return offset;
}

void module_subsys_init(void)
{
    g_module_list = NULL;
    printk("<6>[  0.000000] LKM: Loadable Kernel Module subsystem initialized\n");
}

int64_t sys_init_module(void *umod, unsigned long len, const char *uargs)
{
    if (!umod || len < sizeof(Elf64_Ehdr)) {
        return -EINVAL;
    }

    const uint8_t *raw = (const uint8_t *)umod;
    const Elf64_Ehdr *eh = (const Elf64_Ehdr *)raw;

    /* 1. Verify ELF Identification */
    if (eh->e_ident[EI_MAG0] != ELFMAG0 || eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 || eh->e_ident[EI_MAG3] != ELFMAG3) {
        printk("<3>[LKM] Not a valid ELF file\n");
        return -ENOEXEC;
    }

    if (eh->e_ident[EI_CLASS] != ELFCLASS64 || eh->e_ident[EI_DATA] != ELFDATA2LSB) {
        printk("<3>[LKM] Module must be 64-bit Little Endian\n");
        return -ENOEXEC;
    }

    if (eh->e_type != ET_REL || eh->e_machine != EM_X86_64) {
        printk("<3>[LKM] Module must be x86-64 relocatable object (ET_REL)\n");
        return -ENOEXEC;
    }

    if (eh->e_shoff + eh->e_shnum * sizeof(Elf64_Shdr) > len) {
        return -EINVAL;
    }

    const Elf64_Shdr *shdrs = (const Elf64_Shdr *)(raw + eh->e_shoff);

    /* Locate section header string table */
    if (eh->e_shstrndx >= eh->e_shnum) {
        return -ENOEXEC;
    }
    const char *shstrtab = (const char *)(raw + shdrs[eh->e_shstrndx].sh_offset);

    /* 2. Calculate total size required for SHF_ALLOC sections */
    size_t total_alloc_size = 0;
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        if (shdrs[i].sh_flags & SHF_ALLOC) {
            uint64_t align = shdrs[i].sh_addralign ? shdrs[i].sh_addralign : 1;
            total_alloc_size = (total_alloc_size + align - 1) & ~(align - 1);
            total_alloc_size += shdrs[i].sh_size;
        }
    }

    if (total_alloc_size == 0 || total_alloc_size > 16 * 1024 * 1024) {
        return -EINVAL;
    }

    /* Allocate contiguous kernel memory for module code & data in module higher-half space */
    uint8_t *core_mem = (uint8_t *)module_alloc(total_alloc_size);
    if (!core_mem) {
        return -ENOMEM;
    }
    memset(core_mem, 0, total_alloc_size);

    /* Track loaded addresses for each section */
    uint64_t sec_load_addrs[128];
    memset(sec_load_addrs, 0, sizeof(sec_load_addrs));

    size_t current_offset = 0;
    for (uint16_t i = 0; i < eh->e_shnum && i < 128; i++) {
        if (shdrs[i].sh_flags & SHF_ALLOC) {
            uint64_t align = shdrs[i].sh_addralign ? shdrs[i].sh_addralign : 1;
            current_offset = (current_offset + align - 1) & ~(align - 1);

            uint8_t *dest = core_mem + current_offset;
            sec_load_addrs[i] = (uint64_t)(uintptr_t)dest;

            if (shdrs[i].sh_type != SHT_NOBITS) {
                if (shdrs[i].sh_offset + shdrs[i].sh_size <= len) {
                    memcpy(dest, raw + shdrs[i].sh_offset, shdrs[i].sh_size);
                }
            } else {
                memset(dest, 0, shdrs[i].sh_size);
            }
            current_offset += shdrs[i].sh_size;
        }
    }

    /* 3. Find Symbol Table and String Table */
    const Elf64_Shdr *symtab_sh = NULL;
    const Elf64_Shdr *strtab_sh = NULL;
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        if (shdrs[i].sh_type == SHT_SYMTAB) {
            symtab_sh = &shdrs[i];
            if (shdrs[i].sh_link < eh->e_shnum) {
                strtab_sh = &shdrs[shdrs[i].sh_link];
            }
            break;
        }
    }

    if (!symtab_sh || !strtab_sh) {
        module_free(core_mem, total_alloc_size);
        return -ENOEXEC;
    }

    const Elf64_Sym *syms = (const Elf64_Sym *)(raw + symtab_sh->sh_offset);
    uint32_t num_syms = symtab_sh->sh_size / sizeof(Elf64_Sym);
    const char *strtab = (const char *)(raw + strtab_sh->sh_offset);

    /* 4. Apply Relocations (SHT_RELA) */
    for (uint16_t i = 0; i < eh->e_shnum; i++) {
        if (shdrs[i].sh_type != SHT_RELA) {
            continue;
        }

        uint32_t target_sec_idx = shdrs[i].sh_info;
        if (target_sec_idx >= eh->e_shnum || !(shdrs[target_sec_idx].sh_flags & SHF_ALLOC)) {
            continue;
        }

        uint64_t target_sec_addr = sec_load_addrs[target_sec_idx];
        const Elf64_Rela *relas = (const Elf64_Rela *)(raw + shdrs[i].sh_offset);
        uint32_t num_relas = shdrs[i].sh_size / sizeof(Elf64_Rela);

        for (uint32_t r = 0; r < num_relas; r++) {
            uint32_t sym_idx = (uint32_t)ELF64_R_SYM(relas[r].r_info);
            uint32_t rel_type = (uint32_t)ELF64_R_TYPE(relas[r].r_info);

            if (sym_idx >= num_syms) {
                module_free(core_mem, total_alloc_size);
                return -ENOEXEC;
            }

            const Elf64_Sym *sym = &syms[sym_idx];
            uint64_t sym_val = 0;

            if (sym->st_shndx == SHN_UNDEF) {
                const char *sname = strtab + sym->st_name;
                void *resolved = kallsyms_lookup_name(sname);
                if (!resolved) {
                    printk("<3>[LKM] Unknown symbol '%s' in module\n", sname);
                    module_free(core_mem, total_alloc_size);
                    return -ENOENT;
                }
                sym_val = (uint64_t)(uintptr_t)resolved;
            } else if (sym->st_shndx == SHN_ABS) {
                sym_val = sym->st_value;
            } else if (sym->st_shndx < eh->e_shnum) {
                sym_val = sec_load_addrs[sym->st_shndx] + sym->st_value;
            }

            uint64_t loc = target_sec_addr + relas[r].r_offset;

            switch (rel_type) {
                case R_X86_64_NONE:
                    break;
                case R_X86_64_64:
                    *(uint64_t *)loc = sym_val + relas[r].r_addend;
                    break;
                case R_X86_64_32:
                    *(uint32_t *)loc = (uint32_t)(sym_val + relas[r].r_addend);
                    break;
                case R_X86_64_32S:
                    *(int32_t *)loc = (int32_t)(sym_val + relas[r].r_addend);
                    break;
                case R_X86_64_PC32:
                case R_X86_64_PLT32: {
                    int64_t val = (int64_t)(sym_val + relas[r].r_addend) - (int64_t)loc;
                    if (val < -0x80000000LL || val > 0x7FFFFFFFLL) {
                        printk("<3>[LKM] Relocation overflow for PC32/PLT32: offset %lld out of 32-bit range\n", (long long)val);
                        module_free(core_mem, total_alloc_size);
                        return -ENOEXEC;
                    }
                    *(uint32_t *)loc = (uint32_t)val;
                    break;
                }
                default:
                    printk("<3>[LKM] Unsupported relocation type %u\n", rel_type);
                    module_free(core_mem, total_alloc_size);
                    return -ENOEXEC;
            }
        }
    }

    /* 5. Locate init_module and cleanup_module */
    module_init_fn init_fn = NULL;
    module_cleanup_fn cleanup_fn = NULL;

    for (uint32_t s = 0; s < num_syms; s++) {
        const char *sname = strtab + syms[s].st_name;
        if (strcmp(sname, "init_module") == 0) {
            if (syms[s].st_shndx < eh->e_shnum) {
                init_fn = (module_init_fn)(uintptr_t)(sec_load_addrs[syms[s].st_shndx] + syms[s].st_value);
            }
        } else if (strcmp(sname, "cleanup_module") == 0) {
            if (syms[s].st_shndx < eh->e_shnum) {
                cleanup_fn = (module_cleanup_fn)(uintptr_t)(sec_load_addrs[syms[s].st_shndx] + syms[s].st_value);
            }
        }
    }

    /* 6. Extract module name */
    char mod_name[MODULE_NAME_LEN];
    mod_name[0] = '\0';
    if (uargs && uargs[0]) {
        /* Parse name=... or first word */
        const char *p = uargs;
        if (strncmp(p, "name=", 5) == 0) {
            p += 5;
        }
        size_t idx = 0;
        while (*p && *p != ' ' && idx < sizeof(mod_name) - 1) {
            mod_name[idx++] = *p++;
        }
        mod_name[idx] = '\0';
    }

    if (mod_name[0] == '\0') {
        /* Try to deduce name from .gnu.linkonce / section or default */
        for (uint16_t i = 0; i < eh->e_shnum; i++) {
            const char *sec_name = shstrtab + shdrs[i].sh_name;
            if (strncmp(sec_name, ".data.", 6) == 0) {
                strncpy(mod_name, sec_name + 6, sizeof(mod_name) - 1);
                break;
            }
        }
        if (mod_name[0] == '\0') {
            strncpy(mod_name, "unnamed_module", sizeof(mod_name) - 1);
        }
    }

    if (module_find(mod_name)) {
        printk("<3>[LKM] Module '%s' is already loaded\n", mod_name);
        module_free(core_mem, total_alloc_size);
        return -EEXIST;
    }

    /* 7. Create Module Descriptor */
    struct module *mod = (struct module *)kzalloc(sizeof(struct module), GFP_KERNEL);
    if (!mod) {
        module_free(core_mem, total_alloc_size);
        return -ENOMEM;
    }

    strncpy(mod->name, mod_name, sizeof(mod->name) - 1);
    mod->core_base = core_mem;
    mod->core_size = total_alloc_size;
    mod->init = init_fn;
    mod->cleanup = cleanup_fn;
    mod->refcnt = 0;
    mod->state = MODULE_STATE_COMING;

    /* 8. Call init_module */
    if (init_fn) {
        int err = init_fn();
        if (err != 0) {
            printk("<3>[LKM] Module '%s' init_module failed: %d\n", mod->name, err);
            module_free(core_mem, total_alloc_size);
            kfree(mod);
            return err;
        }
    }

    mod->state = MODULE_STATE_LIVE;

    /* Link into global module list */
    mod->next = g_module_list;
    g_module_list = mod;

    printk("<6>[LKM] Loaded module '%s' (%lu bytes, core at 0x%llx)\n",
           mod->name, (unsigned long)mod->core_size, (unsigned long long)(uintptr_t)mod->core_base);

    return 0;
}

int64_t sys_delete_module(const char *name, unsigned int flags)
{
    if (!name) return -EINVAL;

    struct module **curr = &g_module_list;
    while (*curr) {
        struct module *mod = *curr;
        if (strcmp(mod->name, name) == 0) {
            if (mod->refcnt > 0 && !(flags & 1)) {
                printk("<3>[LKM] Cannot remove '%s': module is in use (refcnt=%d)\n", mod->name, mod->refcnt);
                return -EBUSY;
            }

            mod->state = MODULE_STATE_GOING;
            if (mod->cleanup) {
                mod->cleanup();
            }

            *curr = mod->next;

            printk("<6>[LKM] Unloaded module '%s'\n", mod->name);
            module_free(mod->core_base, mod->core_size);
            kfree(mod);
            return 0;
        }
        curr = &(*curr)->next;
    }

    printk("<3>[LKM] Module '%s' not found for removal\n", name);
    return -ENOENT;
}
