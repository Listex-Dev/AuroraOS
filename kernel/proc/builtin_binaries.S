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
