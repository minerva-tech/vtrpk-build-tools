################################################################################
#
# ti-dvsdk-dm365
#
################################################################################
TI_DVSDK_DM365_VERSION:=4.02.00.06
TI_DVSDK_DM365_FILE_VERSION_MAJOR:=4_02
TI_DVSDK_DM365_FILE_VERSION:=$(TI_DVSDK_DM365_FILE_VERSION_MAJOR)_00_06

TI_DVSDK_DM365_SOURCE:=dvsdk_dm365-evm_$(TI_DVSDK_DM365_VERSION).tar.gz
TI_DVSDK_DM365_SOURCE_BIN:=dvsdk_dm365-evm_$(TI_DVSDK_DM365_FILE_VERSION)_setuplinux
TI_DVSDK_DM365_SOURCE_BIN_FOLDER:=ti-dvsdk_dm365-evm_$(TI_DVSDK_DM365_FILE_VERSION)
TI_DVSDK_DM365_SITE:=http://software-dl.ti.com/dsps/dsps_public_sw/sdo_sb/targetcontent/dvsdk/DVSDK_$(TI_DVSDK_DM365_FILE_VERSION_MAJOR)/latest/exports
TI_DVSDK_DM365_DIR:=$(BUILD_DIR)/ti-dvsdk-dm365-$(TI_DVSDK_DM365_VERSION)

TI_DVSDK_DM365_INSTALL_STAGING = NO
TI_DVSDK_DM365_DEPENDENCIES = linux alsa-lib

# The DVSDK installer from TI will REFUSE to install itself if it does not find
# the right version of the CodeSourcery compiler. This package will download
# then extract the CodeSourcery compiler to a temp directory and keep it there 
# just long enough to run the DVSDK installer.
# It's not the best solution but, for the moment, it's the only one that works.
TI_DVSDK_DM365_CS_VERSION_MAJOR=2009q1
TI_DVSDK_DM365_CS_VERSION=$(TI_DVSDK_DM365_CS_VERSION_MAJOR)-203
TI_DVSDK_DM365_CS_FILE=arm-$(TI_DVSDK_DM365_CS_VERSION)-arm-none-linux-gnueabi-i686-pc-linux-gnu.tar.bz2
TI_DVSDK_DM365_CS_SITE=http://www.codesourcery.com/public/gnu_toolchain/arm-none-linux-gnueabi
TI_DVSDK_DM365_CS_DIR=arm-$(TI_DVSDK_DM365_CS_VERSION_MAJOR)

# Extracting the DVSDK unattended, requires a python script.
TI_DVSDK_DM365_EXTRACT=$(TOPDIR)/package/ti/ti-dvsdk-dm365/ti-dvsdk-dm365-extract.py

# Some directory settings will be required by other tools. 
TI_DVSDK_DM365_CE_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 CE_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_CODEC_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 CODEC_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_DMAI_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 DMAI_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_DVTB_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 DVTB_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_FC_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 FC_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_DM365MM_MODULE_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 DM365MM_MODULE_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_XDAIS_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 XDAIS_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_XDC_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 XDC_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`
TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR:=$(TI_DVSDK_DM365_DIR)/`grep -m 1 LINUXUTILS_INSTALL_DIR $(TI_DVSDK_DM365_DIR)/Rules.make | cut -f2 -d'/'`

# Select what will be built in the DVSDK (not using build all)
#TI_DVSDK_DM365_TARGETS=cmem irq edma dm365mm dmai
TI_DVSDK_DM365_TARGETS=cmem irq edma

# Where to install the kernel modules
$(DL_DIR)/$(TI_DVSDK_DM365_CS_FILE):
	$(call DOWNLOAD,$(TI_DVSDK_DM365_CS_SITE),$(TI_DVSDK_DM365_CS_FILE))

# NOTE:
# Archive contains hardlinks and it may fails on some filesystem. Ignoring
# return values will adding the missing symlink will resolve the issue.
# Anyway, CodeSourcery is not used in this package.
$(DL_DIR)/$(TI_DVSDK_DM365_SOURCE): $(DL_DIR)/$(TI_DVSDK_DM365_CS_FILE)
	$(call DOWNLOAD,$(TI_DVSDK_DM365_SITE),$(TI_DVSDK_DM365_SOURCE_BIN))
	- (cd $(DL_DIR);tar -xf $(DL_DIR)/$(TI_DVSDK_DM365_CS_FILE))
	( if [ ! -e $(DL_DIR)/$(TI_DVSDK_DM365_CS_DIR)/bin/arm-none-linux-gnueabi-gcc ]; then\
			ln -s $(DL_DIR)/$(TI_DVSDK_DM365_CS_DIR)/bin/arm-none-linux-gnueabi-gcc-4.3.3 $(DL_DIR)/$(TI_DVSDK_DM365_CS_DIR)/bin/arm-none-linux-gnueabi-gcc;\
		fi\
	)
	chmod +x $(DL_DIR)/$(TI_DVSDK_DM365_SOURCE_BIN)
	python $(TI_DVSDK_DM365_EXTRACT) \
		$(DL_DIR)/$(TI_DVSDK_DM365_SOURCE_BIN) \
		$(DL_DIR)/$(TI_DVSDK_DM365_CS_DIR)/bin \
		$(DL_DIR)/$(TI_DVSDK_DM365_SOURCE_BIN_FOLDER)
	find $(DL_DIR)/$(TI_DVSDK_DM365_SOURCE_BIN_FOLDER) -type f -not -readable -exec sudo chmod gou+r {} \;
	find $(DL_DIR)/$(TI_DVSDK_DM365_SOURCE_BIN_FOLDER) -type d -not -executable -exec sudo chmod gou+rx {} \;
	(cd $(DL_DIR); \
		tar -czf $(TI_DVSDK_DM365_SOURCE) $(TI_DVSDK_DM365_SOURCE_BIN_FOLDER)\
	)
	rm -rf $(DL_DIR)/$(TI_DVSDK_DM365_SOURCE_BIN_FOLDER)
	rm -rf $(DL_DIR)/$(TI_DVSDK_DM365_CS_DIR)


define TI_DVSDK_DM365_CONFIGURE_CMDS
	$(SED) s:DVSDK_INSTALL_DIR=.*:DVSDK_INSTALL_DIR=$(@D):g $(@D)/Rules.make
	$(SED) s:LINUXKERNEL_INSTALL_DIR=.*:LINUXKERNEL_INSTALL_DIR=$(BUILD_DIR)/linux-$(LINUX_VERSION):g $(@D)/Rules.make
	$(SED) s:CSTOOL_DIR=.*:CSTOOL_DIR=$(dir $(TARGET_CROSS))/..:g $(@D)/Rules.make
	$(SED) s:CSTOOL_PREFIX=.*:CSTOOL_PREFIX=$(TARGET_CROSS):g $(@D)/Rules.make
	$(SED) s:EXEC_DIR=.*:EXEC_DIR=$(STAGING_DIR)/usr/bin:g $(@D)/Rules.make
	$(SED) s:LINUXLIBS_INSTALL_DIR=.*:LINUXLIBS_INSTALL_DIR=$(TARGET_DIR)/usr:g $(@D)/Rules.make
	for i in $(TI_DVSDK_DM365_TARGETS) ; do \
		$(MAKE) -C $(@D) $${i}_clean; \
	done
	$(SED) s:^GCC_LD_FLAGS.*:\&\ -L$(TARGET_DIR)/lib:g $(TI_DVSDK_DM365_DMAI_INSTALL_DIR)/packages/ti/sdo/dmai/apps/Makefile.app
endef

#define TI_DVSDK_DM365_BUILD_CMDS
#	for i in $(TI_DVSDK_DM365_TARGETS) ; do \
#		$(MAKE) -C $(@D) $${i}; \
#	done
#endef

define TI_DVSDK_DM365_BUILD_CMDS
	for i in $(TI_DVSDK_DM365_TARGETS) ; do \
		(cd $(@D)/linuxutils*/packages/ti/sdo/linuxutils/$${i}/src/module; \
		$(MAKE) MVTOOL_PREFIX=$(TARGET_CROSS) LINUXKERNEL_INSTALL_DIR=$(LINUX_DIR) release); \
	done
	#make -C $(@D) dm365mm
	#make -C $(@D) dmai
endef

define TI_DVSDK_DM365_INSTALL_TARGET_CMDS
	$(INSTALL) -d $(TARGET_DIR)/lib/modules/dsp
	#$(INSTALL) -d $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/kernel/drivers/dsp
	$(INSTALL) $(TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/src/module/cmemk.ko $(TARGET_DIR)/lib/modules/dsp/cmemk.ko
	$(INSTALL) $(TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/irq/src/module/irqk.ko $(TARGET_DIR)/lib/modules/dsp/irqk.ko
	$(INSTALL) $(TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/edma/src/module/edmak.ko $(TARGET_DIR)/lib/modules/dsp/edmak.ko
	#$(INSTALL) -d $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/kernel/drivers/dsp
	#$(INSTALL) $(TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/cmem/src/module/cmemk.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/kernel/drivers/dsp/cmemk.ko
	#$(INSTALL) $(TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/irq/src/module/irqk.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/kernel/drivers/dsp/irqk.ko
	#$(INSTALL) $(TI_DVSDK_DM365_LINUXUTILS_INSTALL_DIR)/packages/ti/sdo/linuxutils/edma/src/module/edmak.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/kernel/drivers/dsp/edmak.ko
	#$(INSTALL) $(TI_DVSDK_DM365_DM365MM_MODULE_INSTALL_DIR)/module/dm365mmap.ko $(TARGET_DIR)/lib/modules/$(LINUX_VERSION_PROBED)/kernel/drivers/dsp/dm365mmap.ko
endef

ti-dvsdk-dm365-source: $(DL_DIR)/$(TI_DVSDK_DM365_SOURCE)

$(eval $(generic-package))
