#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$DIR"

make kernel

echo "=========================================================="
echo " Starting AuroraOS in QEMU (Press Ctrl-A X to exit)"
echo "=========================================================="

qemu-system-x86_64 \
    -machine q35 -m 512M -smp 1 \
    -kernel build/aurora.bin \
    -serial stdio \
    -display none \
    -no-reboot -no-shutdown
