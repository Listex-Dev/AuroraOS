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
.global bin_mknod_start, bin_mknod_end
.global bin_fdisk_start, bin_fdisk_end
.global bin_mount_start, bin_mount_end
.global bin_test_m12_start, bin_test_m12_end
.global bin_ping_start, bin_ping_end
.global bin_test_m13_start, bin_test_m13_end
.global bin_dhclient_start, bin_dhclient_end
.global bin_nslookup_start, bin_nslookup_end
.global bin_ifconfig_start, bin_ifconfig_end
.global bin_netstat_start, bin_netstat_end
.global bin_wget_start, bin_wget_end
.global bin_test_m14_start, bin_test_m14_end

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

.align 16
bin_mknod_start:
    .incbin "build/bin/mknod"
bin_mknod_end:

.align 16
bin_fdisk_start:
    .incbin "build/bin/fdisk"
bin_fdisk_end:

.align 16
bin_mount_start:
    .incbin "build/bin/mount"
bin_mount_end:

.align 16
bin_test_m12_start:
    .incbin "build/bin/test_m12"
bin_test_m12_end:

.align 16
bin_ping_start:
    .incbin "build/bin/ping"
bin_ping_end:

.align 16
bin_test_m13_start:
    .incbin "build/bin/test_m13"
bin_test_m13_end:

.align 16
bin_dhclient_start:
    .incbin "build/bin/dhclient"
bin_dhclient_end:

.align 16
bin_nslookup_start:
    .incbin "build/bin/nslookup"
bin_nslookup_end:

.align 16
bin_ifconfig_start:
    .incbin "build/bin/ifconfig"
bin_ifconfig_end:

.align 16
bin_netstat_start:
    .incbin "build/bin/netstat"
bin_netstat_end:

.align 16
bin_wget_start:
    .incbin "build/bin/wget"
bin_wget_end:

.align 16
bin_test_m14_start:
    .incbin "build/bin/test_m14"
bin_test_m14_end:

