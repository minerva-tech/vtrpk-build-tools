###############################################################################
#
# Linux kernel target
#
###############################################################################
LINUX_VERSION=$(call qstrip,$(BR2_LINUX_KERNEL_VERSION))
LINUX_LICENSE = GPLv2
LINUX_LICENSE_FILES = COPYING

# Compute LINUX_SOURCE and LINUX_SITE from the configuration
ifeq ($(LINUX_VERSION),custom)
LINUX_TARBALL = $(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_TARBALL_LOCATION))
LINUX_SITE = $(dir $(LINUX_TARBALL))
LINUX_SOURCE = $(notdir $(LINUX_TARBALL))
else ifeq ($(BR2_LINUX_KERNEL_CUSTOM_GIT),y)
LINUX_SITE = $(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_GIT_REPO_URL))
LINUX_SITE_METHOD = git
else
LINUX_SOURCE = linux-$(LINUX_VERSION).tar.bz2
# In X.Y.Z, get X and Y. We replace dots and dashes by spaces in order
# to use the $(word) function. We support versions such as 3.1,
# 2.6.32, 2.6.32-rc1, 3.0-rc6, etc.
ifeq ($(findstring x2.6.,x$(LINUX_VERSION)),x2.6.)
LINUX_SITE = $(BR2_KERNEL_MIRROR)/linux/kernel/v2.6/
else
LINUX_SITE = $(BR2_KERNEL_MIRROR)/linux/kernel/v3.x/
endif
# release candidates are in testing/ subdir
ifneq ($(findstring -rc,$(LINUX_VERSION)),)
LINUX_SITE := $(LINUX_SITE)testing/
endif # -rc
endif

LINUX_PATCHES = $(call qstrip,$(BR2_LINUX_KERNEL_PATCH))

LINUX_INSTALL_IMAGES = YES
LINUX_DEPENDENCIES  += host-module-init-tools

ifeq ($(BR2_LINUX_KERNEL_UBOOT_IMAGE),y)
	LINUX_DEPENDENCIES += host-uboot-tools
endif

ifeq ($(BR2_TARGET_ROOTFS_CPIO),y)
	LINUX_DEPENDENCIES += rootfs-cpio
endif

LINUX_MAKE_FLAGS = \
	HOSTCC="$(HOSTCC)" \
	HOSTCFLAGS="$(HOSTCFLAGS)" \
	ARCH=$(KERNEL_ARCH) \
	INSTALL_MOD_PATH=$(TARGET_DIR) \
	CROSS_COMPILE="$(CCACHE) $(TARGET_CROSS)" \
	DEPMOD=$(HOST_DIR)/usr/sbin/depmod

# Get the real Linux version, which tells us where kernel modules are
# going to be installed in the target filesystem.
LINUX_VERSION_PROBED = $(shell $(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) --no-print-directory -s kernelrelease)

ifeq ($(BR2_LINUX_KERNEL_USE_INTREE_DTS),y)
KERNEL_DTS_NAME = $(BR2_LINUX_KERNEL_INTREE_DTS_NAME)
else ifeq ($(BR2_LINUX_KERNEL_USE_CUSTOM_DTS),y)
KERNEL_DTS_NAME = $(basename $(notdir $(BR2_LINUX_KERNEL_CUSTOM_DTS_PATH)))
endif

ifeq ($(BR2_LINUX_KERNEL_APPENDED_DTB),y)
ifneq ($(words $(KERNEL_DTS_NAME)),1)
$(error Kernel with appended device tree needs exactly one DTS source.\
  Check BR2_LINUX_KERNEL_INTREE_DTS_NAME or BR2_LINUX_KERNEL_CUSTOM_DTS_PATH.)
endif
endif

ifeq ($(BR2_LINUX_KERNEL_IMAGE_TARGET_CUSTOM),y)
LINUX_IMAGE_NAME=$(call qstrip,$(BR2_LINUX_KERNEL_IMAGE_TARGET_NAME))
else
ifeq ($(BR2_LINUX_KERNEL_UIMAGE),y)
LINUX_IMAGE_NAME=uImage
else ifeq ($(BR2_LINUX_KERNEL_APPENDED_UIMAGE),y)
LINUX_IMAGE_NAME=uImage
else ifeq ($(BR2_LINUX_KERNEL_BZIMAGE),y)
LINUX_IMAGE_NAME=bzImage
else ifeq ($(BR2_LINUX_KERNEL_ZIMAGE),y)
LINUX_IMAGE_NAME=zImage
else ifeq ($(BR2_LINUX_KERNEL_APPENDED_ZIMAGE),y)
LINUX_IMAGE_NAME=zImage
else ifeq ($(BR2_LINUX_KERNEL_CUIMAGE),y)
LINUX_IMAGE_NAME=cuImage.$(KERNEL_DTS_NAME)
else ifeq ($(BR2_LINUX_KERNEL_SIMPLEIMAGE),y)
LINUX_IMAGE_NAME=simpleImage.$(KERNEL_DTS_NAME)
else ifeq ($(BR2_LINUX_KERNEL_LINUX_BIN),y)
LINUX_IMAGE_NAME=linux.bin
else ifeq ($(BR2_LINUX_KERNEL_VMLINUX_BIN),y)
LINUX_IMAGE_NAME=vmlinux.bin
else ifeq ($(BR2_LINUX_KERNEL_VMLINUX),y)
LINUX_IMAGE_NAME=vmlinux
else ifeq ($(BR2_LINUX_KERNEL_VMLINUZ),y)
LINUX_IMAGE_NAME=vmlinuz
endif
endif

ifeq ($(BR2_LINUX_KERNEL_APPENDED_DTB),y)
LINUX_IMAGE_TARGET=zImage
else
LINUX_IMAGE_TARGET=$(LINUX_IMAGE_NAME)
endif

# Compute the arch path, since i386 and x86_64 are in arch/x86 and not
# in arch/$(KERNEL_ARCH). Even if the kernel creates symbolic links
# for bzImage, arch/i386 and arch/x86_64 do not exist when copying the
# defconfig file.
ifeq ($(KERNEL_ARCH),i386)
KERNEL_ARCH_PATH=$(LINUX_DIR)/arch/x86
else ifeq ($(KERNEL_ARCH),x86_64)
KERNEL_ARCH_PATH=$(LINUX_DIR)/arch/x86
else
KERNEL_ARCH_PATH=$(LINUX_DIR)/arch/$(KERNEL_ARCH)
endif

ifeq ($(BR2_LINUX_KERNEL_VMLINUX),y)
LINUX_IMAGE_PATH=$(LINUX_DIR)/$(LINUX_IMAGE_NAME)
else ifeq ($(BR2_LINUX_KERNEL_VMLINUZ),y)
LINUX_IMAGE_PATH=$(LINUX_DIR)/$(LINUX_IMAGE_NAME)
else
ifeq ($(KERNEL_ARCH),avr32)
LINUX_IMAGE_PATH=$(KERNEL_ARCH_PATH)/boot/images/$(LINUX_IMAGE_NAME)
else
LINUX_IMAGE_PATH=$(KERNEL_ARCH_PATH)/boot/$(LINUX_IMAGE_NAME)
endif
endif # BR2_LINUX_KERNEL_VMLINUX

define LINUX_DOWNLOAD_PATCHES
	$(if $(LINUX_PATCHES),
		@$(call MESSAGE,"Download additional patches"))
	$(foreach patch,$(filter ftp://% http://%,$(LINUX_PATCHES)),\
		$(call DOWNLOAD,$(patch))$(sep))
endef

LINUX_POST_DOWNLOAD_HOOKS += LINUX_DOWNLOAD_PATCHES

define LINUX_APPLY_PATCHES
	for p in $(LINUX_PATCHES) ; do \
		if echo $$p | grep -q -E "^ftp://|^http://" ; then \
			support/scripts/apply-patches.sh $(@D) $(DL_DIR) `basename $$p` ; \
		elif test -d $$p ; then \
			support/scripts/apply-patches.sh $(@D) $$p linux-\*.patch ; \
		else \
			support/scripts/apply-patches.sh $(@D) `dirname $$p` `basename $$p` ; \
		fi \
	done
endef

LINUX_POST_PATCH_HOOKS += LINUX_APPLY_PATCHES


ifeq ($(BR2_LINUX_KERNEL_USE_DEFCONFIG),y)
KERNEL_SOURCE_CONFIG = $(KERNEL_ARCH_PATH)/configs/$(call qstrip,$(BR2_LINUX_KERNEL_DEFCONFIG))_defconfig
else ifeq ($(BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG),y)
KERNEL_SOURCE_CONFIG = $(BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE)
endif

define LINUX_CONFIGURE_CMDS
	cp $(KERNEL_SOURCE_CONFIG) $(KERNEL_ARCH_PATH)/configs/buildroot_defconfig
	$(TARGET_MAKE_ENV) $(MAKE1) $(LINUX_MAKE_FLAGS) -C $(@D) buildroot_defconfig
	rm $(KERNEL_ARCH_PATH)/configs/buildroot_defconfig
	$(if $(BR2_ARM_EABI),
		$(call KCONFIG_ENABLE_OPT,CONFIG_AEABI,$(@D)/.config),
		$(call KCONFIG_DISABLE_OPT,CONFIG_AEABI,$(@D)/.config))
	# As the kernel gets compiled before root filesystems are
	# built, we create a fake cpio file. It'll be
	# replaced later by the real cpio archive, and the kernel will be
	# rebuilt using the linux26-rebuild-with-initramfs target.
	$(if $(BR2_TARGET_ROOTFS_INITRAMFS),
		touch $(BINARIES_DIR)/rootfs.cpio
		$(call KCONFIG_ENABLE_OPT,CONFIG_BLK_DEV_INITRD,$(@D)/.config)
		$(call KCONFIG_SET_OPT,CONFIG_INITRAMFS_SOURCE,\"$(BINARIES_DIR)/rootfs.cpio\",$(@D)/.config)
		$(call KCONFIG_SET_OPT,CONFIG_INITRAMFS_ROOT_UID,0,$(@D)/.config)
		$(call KCONFIG_SET_OPT,CONFIG_INITRAMFS_ROOT_GID,0,$(@D)/.config)
		$(call KCONFIG_DISABLE_OPT,CONFIG_INITRAMFS_COMPRESSION_NONE,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_INITRAMFS_COMPRESSION_GZIP,$(@D)/.config))
	$(if $(BR2_ROOTFS_DEVICE_CREATION_STATIC),,
		$(call KCONFIG_ENABLE_OPT,CONFIG_DEVTMPFS,$(@D)/.config)
		$(call KCONFIG_ENABLE_OPT,CONFIG_DEVTMPFS_MOUNT,$(@D)/.config))
	$(if $(BR2_ROOTFS_DEVICE_CREATION_DYNAMIC_MDEV),
		$(call KCONFIG_SET_OPT,CONFIG_UEVENT_HELPER_PATH,\"/sbin/mdev\",$(@D)/.config))
	$(if $(BR2_PACKAGE_SYSTEMD),
		$(call KCONFIG_ENABLE_OPT,CONFIG_CGROUPS,$(@D)/.config))
	$(if $(BR2_LINUX_KERNEL_APPENDED_DTB),
		$(call KCONFIG_ENABLE_OPT,CONFIG_ARM_APPENDED_DTB,$(@D)/.config))
	yes '' | $(TARGET_MAKE_ENV) $(MAKE1) $(LINUX_MAKE_FLAGS) -C $(@D) oldconfig
endef

ifeq ($(BR2_LINUX_KERNEL_DTS_SUPPORT),y)
ifeq ($(BR2_LINUX_KERNEL_DTB_IS_SELF_BUILT),)
define LINUX_BUILD_DTB
	$(TARGET_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(@D) $(KERNEL_DTS_NAME).dtb
endef
define LINUX_INSTALL_DTB
	cp $(KERNEL_ARCH_PATH)/boot/$(KERNEL_DTS_NAME).dtb $(BINARIES_DIR)/
endef
endif
endif

ifeq ($(BR2_LINUX_KERNEL_APPENDED_UIMAGE),y)
define LINUX_APPEND_DTB
	cat $(KERNEL_ARCH_PATH)/boot/$(KERNEL_DTS_NAME).dtb >> $(KERNEL_ARCH_PATH)/boot/zImage
	# We need to generate the uImage here after that so that the uImage is
	# generated with the right image size.
	$(TARGET_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(@D) uImage
endef
else ifeq ($(BR2_LINUX_KERNEL_APPENDED_ZIMAGE),y)
define LINUX_APPEND_DTB
	cat $(KERNEL_ARCH_PATH)/boot/$(KERNEL_DTS_NAME).dtb >> $(KERNEL_ARCH_PATH)/boot/zImage
endef
endif

# Compilation. We make sure the kernel gets rebuilt when the
# configuration has changed.
define LINUX_BUILD_CMDS
	$(if $(BR2_LINUX_KERNEL_USE_CUSTOM_DTS),
		cp $(BR2_LINUX_KERNEL_CUSTOM_DTS_PATH) $(KERNEL_ARCH_PATH)/boot/dts/)
	$(TARGET_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(@D) $(LINUX_IMAGE_TARGET)
	@if grep -q "CONFIG_MODULES=y" $(@D)/.config; then 	\
		$(TARGET_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(@D) modules ;	\
	fi
	$(LINUX_BUILD_DTB)
	$(LINUX_APPEND_DTB)
endef


ifeq ($(BR2_LINUX_KERNEL_INSTALL_TARGET),y)
define LINUX_INSTALL_KERNEL_IMAGE_TO_TARGET
	install -m 0644 -D $(LINUX_IMAGE_PATH) $(TARGET_DIR)/boot/$(LINUX_IMAGE_NAME)
endef
endif


define LINUX_INSTALL_HOST_TOOLS
	# Installing dtc (device tree compiler) as host tool, if selected
	if grep -q "CONFIG_DTC=y" $(@D)/.config; then 	\
		$(INSTALL) -D -m 0755 $(@D)/scripts/dtc/dtc $(HOST_DIR)/usr/bin/dtc ;	\
	fi
endef


define LINUX_INSTALL_IMAGES_CMDS
	cp $(LINUX_IMAGE_PATH) $(BINARIES_DIR)
endef

define LINUX_INSTALL_TARGET_CMDS
	$(LINUX_INSTALL_KERNEL_IMAGE_TO_TARGET)
	$(LINUX_INSTALL_DTB)
	# Install modules and remove symbolic links pointing to build
	# directories, not relevant on the target
	@if grep -q "CONFIG_MODULES=y" $(@D)/.config; then 	\
		$(TARGET_MAKE_ENV) $(MAKE1) $(LINUX_MAKE_FLAGS) -C $(@D) 		\
			DEPMOD="$(HOST_DIR)/usr/sbin/depmod" modules_install ;		\
		rm -f $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/build ;		\
		rm -f $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/source ;	\
	fi
	$(LINUX_INSTALL_HOST_TOOLS)
endef

include linux/linux-ext-*.mk

$(eval $(generic-package))

ifeq ($(BR2_LINUX_KERNEL),y)
linux-menuconfig linux-xconfig linux-gconfig linux-nconfig linux26-menuconfig linux26-xconfig linux26-gconfig linux26-nconfig: dirs linux-configure
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) \
		$(subst linux-,,$(subst linux26-,,$@))
	rm -f $(LINUX_DIR)/.stamp_{built,target_installed,images_installed}

linux-savedefconfig linux26-savedefconfig: dirs linux-configure
	$(MAKE) $(LINUX_MAKE_FLAGS) -C $(LINUX_DIR) \
		$(subst linux-,,$(subst linux26-,,$@))

ifeq ($(BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG),y)
linux-update-config linux26-update-config: linux-configure $(LINUX_DIR)/.config
	cp -f $(LINUX_DIR)/.config $(BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE)

linux-update-defconfig linux26-update-defconfig: linux-savedefconfig
	cp -f $(LINUX_DIR)/defconfig $(BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE)
else
linux-update-config linux26-update-config: ;
linux-update-defconfig linux26-update-defconfig: ;
endif
endif

# Support for rebuilding the kernel after the cpio archive has
# been generated in $(BINARIES_DIR)/rootfs.cpio.
$(LINUX_DIR)/.stamp_initramfs_rebuilt: $(LINUX_DIR)/.stamp_target_installed $(LINUX_DIR)/.stamp_images_installed $(BINARIES_DIR)/rootfs.cpio
	@$(call MESSAGE,"Rebuilding kernel with initramfs")
	# Build the kernel.
	$(TARGET_MAKE_ENV) $(MAKE) $(LINUX_MAKE_FLAGS) -C $(@D) $(LINUX_IMAGE_NAME)
	# Copy the kernel image to its final destination
	cp $(LINUX_IMAGE_PATH) $(BINARIES_DIR)
	# If there is a .ub file copy it to the final destination
	test ! -f $(LINUX_IMAGE_PATH).ub || cp $(LINUX_IMAGE_PATH).ub $(BINARIES_DIR)
	$(Q)touch $@

# The initramfs building code must make sure this target gets called
# after it generated the initramfs list of files.
linux-rebuild-with-initramfs linux26-rebuild-with-initramfs: $(LINUX_DIR)/.stamp_initramfs_rebuilt

# Checks to give errors that the user can understand
ifeq ($(filter source,$(MAKECMDGOALS)),)
ifeq ($(BR2_LINUX_KERNEL_USE_DEFCONFIG),y)
ifeq ($(call qstrip,$(BR2_LINUX_KERNEL_DEFCONFIG)),)
$(error No kernel defconfig name specified, check your BR2_LINUX_KERNEL_DEFCONFIG setting)
endif
endif

ifeq ($(BR2_LINUX_KERNEL_USE_CUSTOM_CONFIG),y)
ifeq ($(call qstrip,$(BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE)),)
$(error No kernel configuration file specified, check your BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE setting)
endif
endif

endif
