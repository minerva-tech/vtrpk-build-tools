#!/bin/sh

(cd output/build/linux-custom && rm -f .stamp_built  .stamp_images_installed  .stamp_initramfs_rebuilt .stamp_target_installed)

make || exit 1

