#!/usr/bin/env bash
set -e

IMG=filesystem.img
TD=/tmp/ext2-data

# Prepare source directory
rm -rf $TD
mkdir -p $TD/bin $TD/etc $TD/home $TD/dev

echo "Welcome to μkrn!" > $TD/home/readme.txt
echo "This file lives on an ext2 filesystem." >> $TD/home/readme.txt

echo "root:x:0:0:root:/root:/bin/sh" > $TD/etc/passwd
echo "127.0.0.1 localhost" > $TD/etc/hosts

printf '#!/bin/sh\necho Hello from ext2!\n' > $TD/bin/hello.sh
chmod +x $TD/bin/hello.sh

ls -la $TD/home/ > $TD/home/dirlist.txt 2>/dev/null

# Create ext2 image from directory (properly updates all metadata)
rm -f $IMG
mke2fs -q -d $TD -b 1024 $IMG 32M

rm -rf $TD
echo "Created $IMG"
