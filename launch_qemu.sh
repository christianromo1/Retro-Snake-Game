#!/bin/bash

# This function kills qemu when we're finished debugging
cleanup() {
  killall qemu-system-i386
}

trap cleanup EXIT

qemu-system-i386 -hda rootfs.img

# TERM=xterm gdb -x gdb_os.txt


