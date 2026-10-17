#include <aurora/drivers/m9_test.h>
#include <aurora/drivers/tty.h>
#include <aurora/drivers/console.h>
#include <aurora/drivers/kbd.h>
#include <aurora/termios.h>
#include <aurora/fs/vfs.h>
#include <aurora/syscall.h>
#include <aurora/proc/task.h>
#include <aurora/proc/sched.h>
#include <aurora/mm/pmm.h>
#include <aurora/mm/vmm.h>
#include <aurora/mm/heap.h>
#include <aurora/printk.h>
#include <aurora/string.h>

extern char user_m9_blob_start[];
extern char user_m9_blob_end[];
extern void enter_usermode(uint64_t rip, uint64_t rsp);

#define M9_USER_CODE_VIRT  0x0000000000400000ULL
#define M9_USER_STACK_VIRT 0x00007FFFF0000000ULL

static void user_m9_trampoline(void)
{
    enter_usermode(M9_USER_CODE_VIRT, M9_USER_STACK_VIRT + PAGE_SIZE - 32);
}

bool test_tty_subsystem(void)
{
    printk("<6>[  0.000000] Running Milestone M9 verification test (TTY line discipline, VT0-5, kbd, termios, Ctrl-C)...\n");

    /* 1. Initialize TTY subsystem and Keyboard driver */
    tty_subsys_init();
    kbd_init();

    /* 2. Verify Character Device Nodes in /dev */
    static const char *test_devs[] = {
        "/dev/tty0", "/dev/tty1", "/dev/tty2", "/dev/tty6",
        "/dev/ttyS0", "/dev/console", "/dev/tty"
    };
    for (size_t i = 0; i < sizeof(test_devs) / sizeof(test_devs[0]); i++) {
        int fd = sys_open(test_devs[i], O_RDWR, 0);
        if (fd < 0) {
            printk("<0>[TEST M9] FAILED: Could not open device node %s (err %d)\n", test_devs[i], fd);
            return false;
        }
        struct stat st;
        int stat_err = sys_fstat(fd, &st);
        sys_close(fd);

        if (stat_err != 0 || !S_ISCHR(st.st_mode)) {
            printk("<0>[TEST M9] FAILED: Device %s is not a character device!\n", test_devs[i]);
            return false;
        }
    }
    printk("<6>[TEST M9] OK: All required TTY and Console device nodes verified in /dev!\n");

    /* 3. Verify termios ioctl (TCGETS, TCSETS, TIOCGWINSZ) */
    int fd_tty1 = sys_open("/dev/tty1", O_RDWR, 0);
    if (fd_tty1 < 0) {
        printk("<0>[TEST M9] FAILED: Could not open /dev/tty1 for ioctl test\n");
        return false;
    }

    struct termios term_orig;
    if (sys_ioctl(fd_tty1, TCGETS, &term_orig) != 0) {
        printk("<0>[TEST M9] FAILED: TCGETS ioctl failed on /dev/tty1\n");
        sys_close(fd_tty1);
        return false;
    }

    if (!(term_orig.c_lflag & ICANON) || !(term_orig.c_lflag & ECHO) || term_orig.c_cc[VINTR] != 0x03) {
        printk("<0>[TEST M9] FAILED: Initial termios flags unexpected!\n");
        sys_close(fd_tty1);
        return false;
    }

    /* Switch to raw mode */
    struct termios term_raw = term_orig;
    term_raw.c_lflag &= ~(ICANON | ECHO);
    if (sys_ioctl(fd_tty1, TCSETS, &term_raw) != 0) {
        printk("<0>[TEST M9] FAILED: TCSETS ioctl failed\n");
        sys_close(fd_tty1);
        return false;
    }

    struct termios term_check;
    sys_ioctl(fd_tty1, TCGETS, &term_check);
    if ((term_check.c_lflag & ICANON) || (term_check.c_lflag & ECHO)) {
        printk("<0>[TEST M9] FAILED: Raw mode termios not persisted!\n");
        sys_close(fd_tty1);
        return false;
    }

    /* Restore original canonical termios */
    sys_ioctl(fd_tty1, TCSETS, &term_orig);

    /* Verify TIOCGWINSZ */
    struct winsize ws;
    if (sys_ioctl(fd_tty1, TIOCGWINSZ, &ws) != 0 || ws.ws_col != 80 || ws.ws_row != 25) {
        printk("<0>[TEST M9] FAILED: TIOCGWINSZ reported unexpected dimensions (%ux%u)\n",
               ws.ws_col, ws.ws_row);
        sys_close(fd_tty1);
        return false;
    }
    sys_close(fd_tty1);
    printk("<6>[TEST M9] OK: termios TCGETS, TCSETS, TIOCGWINSZ verified!\n");

    /* 4. Verify Console Output and ANSI Escape Sequence Interpretation */
    int fd_ansi = sys_open("/dev/tty1", O_RDWR, 0);
    const char *ansi_test = "\033[2J\033[5;10H\033[1;32mAuroraOS TTY\033[0m";
    sys_write(fd_ansi, ansi_test, strlen(ansi_test));
    sys_close(fd_ansi);

    uint16_t *vt1_buf = vt_get_buffer(0);
    /* Row 4 (5th line), Col 9 (10th col) should contain 'A' with light green color attribute */
    uint16_t cell = vt1_buf[4 * VGA_WIDTH + 9];
    char ch = (char)(cell & 0xFF);
    uint8_t attr = (uint8_t)(cell >> 8);

    if (ch != 'A' || (attr & 0x0F) != VGA_COLOR_LIGHT_GREEN) {
        printk("<0>[TEST M9] FAILED: ANSI sequence render mismatch (got char '%c', attr 0x%02x, expected 'A', 0x0A)\n",
               ch, attr);
        return false;
    }
    printk("<6>[TEST M9] OK: Console output and ANSI cursor/color sequences verified!\n");

    /* 5. Verify Canonical Line Editing (Backspace & Ctrl-U) */
    int fd_canon = sys_open("/dev/tty2", O_RDWR, 0);
    if (fd_canon < 0) return false;

    /* Feed "helx\blo\n" -> should become "hello\n" */
    tty_input_string(tty_get(2), "helx\blo\n");
    char read_buf[64];
    memset(read_buf, 0, sizeof(read_buf));
    int64_t nread = sys_read(fd_canon, read_buf, sizeof(read_buf) - 1);

    if (nread != 6 || strcmp(read_buf, "hello\n") != 0) {
        printk("<0>[TEST M9] FAILED: Canonical backspace editing failed (read '%s', expected 'hello\\n')\n",
               read_buf);
        sys_close(fd_canon);
        return false;
    }

    /* Feed "discard this line\x15accepted\n" (0x15 = Ctrl-U) */
    tty_input_string(tty_get(2), "discard this line\x15" "accepted\n");
    memset(read_buf, 0, sizeof(read_buf));
    nread = sys_read(fd_canon, read_buf, sizeof(read_buf) - 1);

    if (nread != 9 || strcmp(read_buf, "accepted\n") != 0) {
        printk("<0>[TEST M9] FAILED: Canonical Ctrl-U line kill failed (read '%s', expected 'accepted\\n')\n",
               read_buf);
        sys_close(fd_canon);
        return false;
    }
    sys_close(fd_canon);
    printk("<6>[TEST M9] OK: Line discipline canonical editing (Backspace, Ctrl-U) verified!\n");

    /* 6. Verify Raw Mode (vi Mode: Immediate Byte Input without Newline) */
    int fd_raw = sys_open("/dev/tty3", O_RDWR, 0);
    if (fd_raw < 0) return false;

    struct termios raw_mode;
    sys_ioctl(fd_raw, TCGETS, &raw_mode);
    raw_mode.c_lflag &= ~(ICANON | ECHO);
    sys_ioctl(fd_raw, TCSETS, &raw_mode);

    tty_input_char(tty_get(3), 'x');
    char raw_char = 0;
    int64_t raw_n = sys_read(fd_raw, &raw_char, 1);
    sys_close(fd_raw);

    if (raw_n != 1 || raw_char != 'x') {
        printk("<0>[TEST M9] FAILED: Raw mode single-character read failed (got '%c', len %ld)\n",
               raw_char, (long)raw_n);
        return false;
    }
    printk("<6>[TEST M9] OK: Raw non-canonical mode (vi mode) verified!\n");

    /* 7. Verify Virtual Terminal Switching via Hotkeys (Alt+F1..F6) */
    vt_switch(1); /* Switch to VT2 */
    if (vt_get_active() != 1) {
        printk("<0>[TEST M9] FAILED: Could not switch to VT2!\n");
        return false;
    }

    /* Inject Alt + F1 scancode sequence: 0x38 (Alt make), 0x3B (F1 make) */
    kbd_inject_scancode(0x38);
    kbd_inject_scancode(0x3B);
    kbd_inject_scancode(0x3B | 0x80);
    kbd_inject_scancode(0x38 | 0x80);

    if (vt_get_active() != 0) {
        printk("<0>[TEST M9] FAILED: Keyboard Alt+F1 hotkey did not switch to VT1!\n");
        return false;
    }
    printk("<6>[TEST M9] OK: Virtual terminal switching and keyboard hotkeys verified!\n");

    /* 8. Verify Ring 3 User Mode execution & Ctrl-C (SIGINT) signal handling */
    printk("<6>[TEST M9] Testing Ring 3 User Mode execution & Ctrl-C (SIGINT) handling...\n");
    uintptr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) return false;
    uint64_t *user_pml4 = (uint64_t *)vmm_phys_to_virt(pml4_phys);
    memset(user_pml4, 0, PAGE_SIZE);

    uint64_t *k_pml4 = vmm_get_kernel_pml4();
    for (size_t i = 256; i < 512; i++) {
        user_pml4[i] = k_pml4[i];
    }

    uintptr_t code_phys = pmm_alloc_page();
    uintptr_t stack_phys = pmm_alloc_page();
    vmm_map_page(user_pml4, M9_USER_CODE_VIRT, code_phys, PTE_USER | PTE_WRITABLE);
    vmm_map_page(user_pml4, M9_USER_STACK_VIRT, stack_phys, PTE_USER | PTE_WRITABLE);

    size_t code_len = user_m9_blob_end - user_m9_blob_start;
    memcpy(vmm_phys_to_virt(code_phys), user_m9_blob_start, code_len);
    memset(vmm_phys_to_virt(stack_phys), 0, PAGE_SIZE);

    task_t *utask = task_create("m9_user", user_m9_trampoline);
    if (!utask) return false;

    utask->pml4 = user_pml4;
    utask->cr3 = pml4_phys;
    utask->brk_start = 0x600000ULL;
    utask->brk_current = utask->brk_start;
    sched_add_task(utask);

    /* Let user task run until child sets itself as foreground pgrp on tty1 */
    struct tty *tty1 = tty_get(1);
    for (int iter = 0; iter < 100; iter++) {
        sched_yield();
        if (utask->state == TASK_ZOMBIE || (tty1->pgrp != 0 && tty1->pgrp != utask->pid)) {
            break;
        }
    }

    if (tty1->pgrp == 0 || tty1->pgrp == utask->pid) {
        printk("<0>[TEST M9] FAILED: Child process did not register foreground pgrp on tty1 (pgrp=%d, parent=%d, exit=%d)!\n",
               tty1->pgrp, utask->pid, utask->exit_code);
        return false;
    }

    /* Inject Ctrl-C (0x03) into tty1 */
    tty_input_char(tty1, 0x03);

    /* Yield until parent reaps child and exits */
    while (utask->state != TASK_ZOMBIE) {
        sched_yield();
    }

    if (utask->exit_code != 89) {
        printk("<0>[TEST M9] FAILED: User M9 parent exited with code %d (expected 89)!\n",
               utask->exit_code);
        task_destroy(utask);
        vmm_destroy_user_space(utask->pml4);
        return false;
    }

    task_destroy(utask);
    vmm_destroy_user_space(utask->pml4);
    printk("<6>[TEST M9] OK: Ring 3 User Mode verified Ctrl-C SIGINT delivery and wait4!\n");

    /* 9. Milestone M9 Acceptance Passed */
    printk("<6>[  0.000000] Milestone M9 status: [OK] TTY line discipline, VT0-5, kbd, termios, canonical/raw, Ctrl-C verified.\n");
    return true;
}
