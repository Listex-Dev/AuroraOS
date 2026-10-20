#!/bin/bash
set -e

echo "=== Preparing AuroraOS Root Filesystem (Rootfs) ==="

ROOTFS="build/rootfs"
rm -rf "$ROOTFS"
mkdir -p "$ROOTFS"/{bin,sbin,lib/modules,etc/pkg/trusted,dev,proc,sys,tmp,run,mnt/old_root,old_root,var/log,var/lib/pkg,var/cache/pkg,packages,root,home/aurora,usr/bin,usr/share/man/man1,usr/share/man/man5,usr/share/man/man8}

# 1. Copy userland binaries to /bin and /usr/bin
if [ -d build/bin ]; then
    cp -a build/bin/* "$ROOTFS"/bin/
    cp -a build/bin/* "$ROOTFS"/usr/bin/
fi

if [ -f build/hello.elf ]; then
    cp -a build/hello.elf "$ROOTFS"/bin/hello
fi

# 2. Populate /sbin
for sbin in init fdisk dhclient ifconfig syslogd crond sshd telinit; do
    if [ -f "build/bin/$sbin" ]; then
        cp -a "build/bin/$sbin" "$ROOTFS"/sbin/
    fi
done

# 3. Copy kernel modules to /lib/modules
if [ -f build/lib/modules/test_module.ko ]; then
    cp -a build/lib/modules/test_module.ko "$ROOTFS"/lib/modules/
fi

# 4. Populate /etc configuration files
cat << 'EOF' > "$ROOTFS"/etc/fstab
/dev/sda2   /        ext4     defaults  0 1
proc        /proc    procfs   defaults  0 0
sysfs       /sys     sysfs    defaults  0 0
devtmpfs    /dev     devtmpfs defaults  0 0
tmpfs       /run     tmpfs    defaults  0 0
tmpfs       /tmp     tmpfs    defaults  0 0
EOF

cat << 'EOF' > "$ROOTFS"/etc/passwd
root:x:0:0:root:/root:/bin/sh
aurora:x:1000:1000:Aurora User:/home/aurora:/bin/sh
EOF

cat << 'EOF' > "$ROOTFS"/etc/group
root:x:0:
aurora:x:1000:
wheel:x:10:root,aurora
EOF

cat << 'EOF' > "$ROOTFS"/etc/shadow
root:$6$saltsalt$FCtXpTmG7SvkmMFnnZIotZMsxPcyMf9rYNlYFfsA2Fau3J9awLnj6tPMJW6qxQdpHAgjs0cvMWQ2Ve2HWIdX2/:19000:0:99999:7:::
aurora:$6$saltsalt$VcYMCBfELnOcDK8outogXZyOp5rTpzY5E2LZQKa6N.QAElzmqtqwgRygFvpH/3Mcts8sy/gn7EtOsH5Rw/Xw4/:19000:0:99999:7:::
EOF
chmod 0600 "$ROOTFS"/etc/shadow

echo -e "AuroraOS 2.0 Ultra (x86_64) \\l\n" > "$ROOTFS"/etc/issue
echo -e "\nWelcome to AuroraOS 2.0 Ultra!\nAll systems operating in 64-bit Long Mode.\n" > "$ROOTFS"/etc/motd
echo "aurora" > "$ROOTFS"/etc/hostname

cat << 'EOF' > "$ROOTFS"/etc/inittab
id:3:initdefault:
si::sysinit:/bin/sh
1:2345:respawn:/bin/getty tty1
2:2345:respawn:/bin/getty tty2
3:2345:respawn:/bin/getty tty3
EOF

echo "aurora-os-trusted-key-v1-2026-pkg-signing-secret" > "$ROOTFS"/etc/pkg/trusted/aurora.key
chmod 0600 "$ROOTFS"/etc/pkg/trusted/aurora.key

# 5. Copy manual pages
if [ -d user/share/man ]; then
    cp -r user/share/man/* "$ROOTFS"/usr/share/man/
fi

# 6. Copy test packages
if [ -d build/packages ]; then
    cp -a build/packages/*.aurpkg "$ROOTFS"/packages/ 2>/dev/null || true
fi

# 7. Generate ext4 filesystem image for rootfs partition (48 MB)
echo "=== Generating build/rootfs.ext4 ==="
mke2fs -q -F -d "$ROOTFS" -b 1024 -O ^has_journal build/rootfs.ext4 48128

echo "=== Rootfs generation complete ==="
ls -lh build/rootfs.ext4
