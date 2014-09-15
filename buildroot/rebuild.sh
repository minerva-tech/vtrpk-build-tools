#!/bin/sh

make || exit 1

(cd output/build/linux-custom && rm -f .stamp_built  .stamp_images_installed  .stamp_initramfs_rebuilt .stamp_target_installed)

make || exit 2

