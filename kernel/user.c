/* user.c — User mode (Ring 3) process setup
 * Phase 5: init → shell separation
 * Phase 6: ELF64 user program loader (spawn_file)
 *
 * Kernel launches init (PID 1), init spawns shell (PID 2) via syscall 11.
 * Init or shell can load ELF64 executables from FAT32 via syscall 12 (exec).
 *
 * ELF64 loader pipeline:
 *   FAT32 → VFS → open/read → ELF64 parser → PT_LOAD → alloc pages
 *   → map @ p_vaddr → copy p_filesz → zero-fill p_memsz-p_filesz
 *   → permissions PF_R/PF_W/PF_X → CR3 → user stack → RIP=e_entry → Ring 3
 */
#include <stdint.h>
#include "cpu.h"
#include "sched.h"
#include "mem.h"
#include "vfs.h"
#include "elf.h"

extern const unsigned char init_code_start[];
extern const unsigned char init_code_end[];
extern const unsigned char shell_code_start[];
extern const unsigned char shell_code_end[];

#define USER_CODE_ADDR   0x800000ULL
#define INIT_PHYS        0x800000ULL

/* Maximum ELF file size we can load (16 KB) */
#define ELF_BUF_SIZE 16384

static int next_proc_idx = 0;

static void copy_code(uint64_t phys_addr, const unsigned char *start, const unsigned char *end) {
    uint8_t *dst = (uint8_t *)(unsigned long)phys_addr;
    uint64_t len = end - start;
    for (uint64_t i = 0; i < len; i++)
        dst[i] = start[i];
}

void user_init(void) {
    serial_puts("[*] Creating init process...\n");

    copy_code(INIT_PHYS, init_code_start, init_code_end);
    uint64_t cr3 = create_user_pml4(next_proc_idx++, INIT_PHYS);
    uint64_t stack_page = alloc_page();
    map_page(cr3, USER_STACK_TOP - PAGE_SIZE, stack_page,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    int pid = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3, USER_HEAP_BASE);
    serial_puts("[+] Init PID=1 (phys 0x800000, stack @0xBFF000, heap @0x1000000)\n");
    (void)pid;
}

int spawn_shell(void) {
    serial_puts("[*] Spawning shell process...\n");

    if (next_proc_idx >= MAX_PROCS) return -1;

    /* Allocate a 4KB page for shell code */
    uint64_t code_phys = alloc_page();
    if (!code_phys) return -1;

    copy_code(code_phys, shell_code_start, shell_code_end);

    /* Create page tables — no 2MB code page (pass 0) */
    uint64_t cr3 = create_user_pml4(next_proc_idx++, 0);
    if (!cr3) {
        free_page(code_phys);
        return -1;
    }

    /* Map shell code at USER_CODE_ADDR with a 4KB page */
    if (map_page(cr3, USER_CODE_ADDR, code_phys,
                 PTE_PRESENT | PTE_WRITABLE | PTE_USER) != 0) {
        free_page(code_phys);
        return -1;
    }

    /* Allocate and map stack page */
    uint64_t stack_page = alloc_page();
    if (!stack_page) {
        free_page(code_phys);
        return -1;
    }
    map_page(cr3, USER_STACK_TOP - PAGE_SIZE, stack_page,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    int pid = proc_create_user(USER_CODE_ADDR, USER_STACK_TOP, cr3, USER_HEAP_BASE);
    serial_puts("[+] Shell process spawned\n");
    return pid;
}

/* ===== ELF64 Loader ===== */

/* Validate ELF64 header — magic, class, endianness, type, machine.
 * Returns 0 on success, -1 on failure. */
static int elf_validate_header(const struct elf64_ehdr *eh) {
    if (eh->e_ident[EI_MAG0] != ELFMAG0 ||
        eh->e_ident[EI_MAG1] != ELFMAG1 ||
        eh->e_ident[EI_MAG2] != ELFMAG2 ||
        eh->e_ident[EI_MAG3] != ELFMAG3) {
        serial_puts("[!] ELF: bad magic\n");
        return -1;
    }
    if (eh->e_ident[EI_CLASS] != ELFCLASS64) {
        serial_puts("[!] ELF: not 64-bit\n");
        return -1;
    }
    if (eh->e_ident[EI_DATA] != ELFDATA2LSB) {
        serial_puts("[!] ELF: not little-endian\n");
        return -1;
    }
    if (eh->e_type != ET_EXEC) {
        serial_puts("[!] ELF: not ET_EXEC\n");
        return -1;
    }
    if (eh->e_machine != EM_X86_64) {
        serial_puts("[!] ELF: not x86_64\n");
        return -1;
    }
    if (eh->e_phentsize != sizeof(struct elf64_phdr)) {
        serial_puts("[!] ELF: bad phentsize\n");
        return -1;
    }
    return 0;
}

/* Convert ELF p_flags to page table flags.
 * PF_R → PRESENT | USER
 * PF_W → add WRITABLE
 * PF_X → (no NX bit yet — all pages executable) */
static uint64_t elf_flags_to_pte(uint32_t p_flags) {
    uint64_t flags = PTE_PRESENT | PTE_USER;
    if (p_flags & PF_W) flags |= PTE_WRITABLE;
    return flags;
}

/* Load a single PT_LOAD segment: allocate pages, copy file data, zero-fill bss,
 * map at p_vaddr with correct permissions.
 * Returns 0 on success, -1 on failure (does not clean up on failure — caller handles). */
static int elf_load_segment(const struct elf64_phdr *ph, const uint8_t *elf_buf,
                            int file_size, uint64_t cr3) {
    uint64_t first_page = ph->p_vaddr & ~(PAGE_SIZE - 1);
    uint64_t last_byte  = ph->p_vaddr + ph->p_memsz;
    if (last_byte == 0) return -1;  /* empty segment */
    uint64_t last_page  = (last_byte - 1) & ~(PAGE_SIZE - 1);

    uint64_t pte_flags = elf_flags_to_pte(ph->p_flags);

    serial_puts("  [+] PT_LOAD: vaddr=0x");
    /* quick hex print */
    {
        char hexbuf[20];
        int hi = 0;
        uint64_t v = ph->p_vaddr;
        char tmp[16]; int ti = 0;
        while (v) { tmp[ti++] = "0123456789ABCDEF"[v & 0xF]; v >>= 4; }
        if (ti == 0) { hexbuf[0] = '0'; hi = 1; }
        while (ti) hexbuf[hi++] = tmp[--ti];
        hexbuf[hi] = 0;
        serial_puts(hexbuf);
        serial_puts(" filesz=");
        /* decimal print */
        char dbuf[16]; int di = 0; uint64_t f = ph->p_filesz;
        char dt[16]; int dti = 0;
        while (f) { dt[dti++] = '0' + (f % 10); f /= 10; }
        if (dti == 0) { dbuf[0] = '0'; di = 1; }
        while (dti) dbuf[di++] = dt[--dti];
        dbuf[di] = 0;
        serial_puts(dbuf);
        serial_puts(" memsz=");
        char mbuf[16]; int mi = 0; uint64_t m = ph->p_memsz;
        char mt[16]; int mti = 0;
        while (m) { mt[mti++] = '0' + (m % 10); m /= 10; }
        if (mti == 0) { mbuf[0] = '0'; mi = 1; }
        while (mti) mbuf[mi++] = mt[--mti];
        mbuf[mi] = 0;
        serial_puts(mbuf);
        serial_puts("\n");
    }

    for (uint64_t va = first_page; va <= last_page; va += PAGE_SIZE) {
        /* Allocate a physical page */
        uint64_t phys = alloc_page();
        if (!phys) {
            serial_puts("[!] ELF: alloc_page failed\n");
            return -1;
        }

        /* Zero the entire physical page (handles bss zero-fill) */
        uint8_t *p = (uint8_t *)(unsigned long)phys;
        for (int i = 0; i < (int)PAGE_SIZE; i++)
            p[i] = 0;

        /* Calculate overlap between file data and this page */
        uint64_t file_vstart = ph->p_vaddr;
        uint64_t file_vend   = ph->p_vaddr + ph->p_filesz;
        uint64_t page_start  = va;
        uint64_t page_end    = va + PAGE_SIZE;

        uint64_t copy_start = file_vstart > page_start ? file_vstart : page_start;
        uint64_t copy_end   = file_vend < page_end ? file_vend : page_end;

        if (copy_start < copy_end) {
            uint64_t file_off = ph->p_offset + (copy_start - ph->p_vaddr);
            uint64_t page_off = copy_start - page_start;
            uint64_t copy_len = copy_end - copy_start;

            /* Bounds check */
            if (file_off + copy_len > (uint64_t)file_size) {
                serial_puts("[!] ELF: segment exceeds file\n");
                free_page(phys);
                return -1;
            }

            uint8_t *src = (uint8_t *)(unsigned long)(elf_buf + file_off);
            uint8_t *dst = (uint8_t *)(unsigned long)(phys + page_off);
            for (uint64_t i = 0; i < copy_len; i++)
                dst[i] = src[i];
        }

        /* Map physical page at virtual address with segment permissions */
        if (map_page(cr3, va, phys, pte_flags) != 0) {
            serial_puts("[!] ELF: map_page failed\n");
            free_page(phys);
            return -1;
        }
    }

    return 0;
}

/* spawn_file — Load an ELF64 executable from FAT32 and execute it in Ring 3.
 *
 * Reads the ELF file via VFS, validates the header, iterates over PT_LOAD
 * program headers, allocates and maps pages at each segment's p_vaddr with
 * the correct permissions, copies p_filesz bytes from the file, zero-fills
 * p_memsz - p_filesz bytes (bss), allocates a user stack, and creates a
 * new Ring 3 process with RIP = e_entry.
 *
 * Returns: PID on success, -1 on failure.
 */
int spawn_file(const char *path) {
    serial_puts("[*] ELF loader: ");
    serial_puts(path);
    serial_puts("\n");

    if (next_proc_idx >= MAX_PROCS) {
        serial_puts("[!] spawn_file: max processes reached\n");
        return -1;
    }

    /* Open file via kernel-space VFS */
    int fd = vfs_open(path);
    if (fd < 0) {
        serial_puts("[!] ELF: file not found\n");
        return -1;
    }

    /* Read entire ELF file into kernel buffer */
    static uint8_t elf_buf[ELF_BUF_SIZE];
    int total = 0;
    while (total < (int)sizeof(elf_buf)) {
        int n = vfs_read(fd, elf_buf + total, sizeof(elf_buf) - total);
        if (n <= 0) break;
        total += n;
    }
    vfs_close(fd);

    if (total < (int)sizeof(struct elf64_ehdr)) {
        serial_puts("[!] ELF: file too small for header\n");
        return -1;
    }

    serial_puts("[+] ELF file loaded: ");
    {
        char dbuf[16]; int di = 0; uint64_t f = total;
        char dt[16]; int dti = 0;
        while (f) { dt[dti++] = '0' + (f % 10); f /= 10; }
        if (dti == 0) { dbuf[0] = '0'; di = 1; }
        while (dti) dbuf[di++] = dt[--dti];
        dbuf[di] = 0;
        serial_puts(dbuf);
        serial_puts(" bytes\n");
    }

    /* Validate ELF64 header */
    struct elf64_ehdr *ehdr = (struct elf64_ehdr *)elf_buf;
    if (elf_validate_header(ehdr) != 0) {
        return -1;
    }

    serial_puts("[+] ELF header valid (x86_64 ET_EXEC)\n");
    serial_puts("[+] Entry point: 0x");
    {
        char hexbuf[20]; int hi = 0; uint64_t v = ehdr->e_entry;
        char tmp[16]; int ti = 0;
        while (v) { tmp[ti++] = "0123456789ABCDEF"[v & 0xF]; v >>= 4; }
        if (ti == 0) { hexbuf[0] = '0'; hi = 1; }
        while (ti) hexbuf[hi++] = tmp[--ti];
        hexbuf[hi] = 0;
        serial_puts(hexbuf);
        serial_puts("\n");
    }

    /* Validate program header table fits in buffer */
    if (ehdr->e_phoff + (uint64_t)ehdr->e_phnum * ehdr->e_phentsize > (uint64_t)total) {
        serial_puts("[!] ELF: program headers exceed file\n");
        return -1;
    }

    /* Create process page tables (no 2MB code page — segments mapped individually) */
    uint64_t cr3 = create_user_pml4(next_proc_idx++, 0);
    if (!cr3) {
        serial_puts("[!] ELF: create_user_pml4 failed\n");
        return -1;
    }

    /* Iterate over program headers and load PT_LOAD segments */
    int seg_count = 0;
    for (int i = 0; i < ehdr->e_phnum; i++) {
        struct elf64_phdr *phdr = (struct elf64_phdr *)
            (elf_buf + ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD)
            continue;

        if (elf_load_segment(phdr, elf_buf, total, cr3) != 0) {
            serial_puts("[!] ELF: segment load failed\n");
            return -1;
        }
        seg_count++;
    }

    if (seg_count == 0) {
        serial_puts("[!] ELF: no PT_LOAD segments\n");
        return -1;
    }

    serial_puts("[+] All PT_LOAD segments mapped\n");

    /* Allocate and map user stack page */
    uint64_t stack_page = alloc_page();
    if (!stack_page) {
        serial_puts("[!] ELF: stack alloc failed\n");
        return -1;
    }
    map_page(cr3, USER_STACK_TOP - PAGE_SIZE, stack_page,
             PTE_PRESENT | PTE_WRITABLE | PTE_USER);

    /* Create user process — RIP = ELF entry point */
    int pid = proc_create_user(ehdr->e_entry, USER_STACK_TOP, cr3, USER_HEAP_BASE);
    if (pid < 0) {
        free_page(stack_page);
        serial_puts("[!] ELF: proc_create_user failed\n");
        return -1;
    }

    serial_puts("[+] ELF process spawned\n");
    return pid;
}
