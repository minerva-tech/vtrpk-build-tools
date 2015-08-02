#############################################################
#
# vrtpk
#
#############################################################
VRTPK_SOURCE =
VRTPK_INSTALL_STAGING = NO
VRTPK_INSTALL_TARGET = YES

VRTPK_DEPENDENCIES = mtd

define VRTPK_EXTRACT_CMDS
	cp -af package/vrtpk/*.[c,h] $(VRTPK_DIR)
endef

define VRTPK_BUILD_CMDS
	(cd $(VRTPK_DIR); \
	$(TARGET_CC) $(TARGET_CFLAGS) fw_write.c -lmtd -o fw_write || exit 1; )
endef

define VRTPK_INSTALL_TARGET_CMDS
	cp -fp $(VRTPK_DIR)/fw_write $(TARGET_DIR)/usr/bin
	-$(STRIPCMD) --strip-unneeded $(TARGET_DIR)/usr/bin/fw_write
endef

$(eval $(generic-package))
