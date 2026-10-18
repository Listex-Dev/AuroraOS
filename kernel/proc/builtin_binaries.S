/*
 * Embed builtin user ELF64 binaries for execve
 */

.section .rodata
.global bin_hello_start, bin_hello_end
.global bin_ls_start, bin_ls_end
.global bin_cat_start, bin_cat_end
.global bin_cp_start, bin_cp_end
.global bin_ps_start, bin_ps_end
.global bin_vi_start, bin_vi_end
.global bin_grep_start, bin_grep_end
.global bin_test_m10_start, bin_test_m10_end
.global bin_sleep_start, bin_sleep_end
.global bin_sh_start, bin_sh_end
.global bin_getty_start, bin_getty_end
.global bin_login_start, bin_login_end
.global bin_init_start, bin_init_end
.global bin_test_m11_start, bin_test_m11_end

.align 16
bin_hello_start:
    .incbin "build/hello.elf"
bin_hello_end:

.align 16
bin_ls_start:
    .incbin "build/bin/ls"
bin_ls_end:

.align 16
bin_cat_start:
    .incbin "build/bin/cat"
bin_cat_end:

.align 16
bin_cp_start:
    .incbin "build/bin/cp"
bin_cp_end:

.align 16
bin_ps_start:
    .incbin "build/bin/ps"
bin_ps_end:

.align 16
bin_vi_start:
    .incbin "build/bin/vi"
bin_vi_end:

.align 16
bin_grep_start:
    .incbin "build/bin/grep"
bin_grep_end:

.align 16
bin_test_m10_start:
    .incbin "build/bin/test_m10"
bin_test_m10_end:

.align 16
bin_sleep_start:
    .incbin "build/bin/sleep"
bin_sleep_end:

.align 16
bin_sh_start:
    .incbin "build/bin/sh"
bin_sh_end:

.align 16
bin_getty_start:
    .incbin "build/bin/getty"
bin_getty_end:

.align 16
bin_login_start:
    .incbin "build/bin/login"
bin_login_end:

.align 16
bin_init_start:
    .incbin "build/bin/init"
bin_init_end:

.align 16
bin_test_m11_start:
    .incbin "build/bin/test_m11"
bin_test_m11_end:
