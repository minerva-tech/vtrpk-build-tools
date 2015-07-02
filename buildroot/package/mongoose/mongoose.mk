#############################################################
#
# mongoose
#
#############################################################
MONGOOSE_VERSION:=3.8
MONGOOSE_SOURCE:=mongoose-$(MONGOOSE_VERSION).tgz
MONGOOSE_SITE:=http://mongoose.googlecode.com/files/

MONGOOSE_DEPENDENCIES =

define MONGOOSE_COPY_SRC
	cp -f package/mongoose/*.[c,h] $(@D)
endef

MONGOOSE_POST_PATCH_HOOKS += MONGOOSE_COPY_SRC

define MONGOOSE_BUILD_CMDS
	(cd $(@D); \
	$(MAKE) CFLAGS="$(TARGET_CFLAGS) -lpthread" CC="$(TARGET_CC)" linux; \
	$(TARGET_CC) $(TARGET_CFLAGS) -D_REENTERANT -D__linux__ -std=c99 -I. -DNO_CGI httpd.c http.c mongoose.c -ldl -lpthread -o httpd)
endef

define MONGOOSE_INSTALL_TARGET_CMDS
	#$(INSTALL) -m 755 -D $(@D)/mongoose $(TARGET_DIR)/usr/bin
	#-$(STRIPCMD) $(STRIP_STRIP_UNNEEDED) $(TARGET_DIR)/usr/bin/mongoose
	$(INSTALL) -m 755 -D $(@D)/httpd $(TARGET_DIR)/usr/bin
	cp -af package/mongoose/*.sh $(TARGET_DIR)/usr/bin
	-$(STRIPCMD) $(STRIP_STRIP_UNNEEDED) $(TARGET_DIR)/usr/bin/httpd
	mkdir -p $(TARGET_DIR)/var/www
endef

define MONGOOSE_CLEAN_CMDS
	$(MAKE) -C $(@D) clean
endef

define MONGOOSE_UNINSTALL_TARGET_CMDS
	rm -f $(TARGET_DIR)/usr/bin/mongoose
	rm -f $(TARGET_DIR)/usr/bin/httpd
	rm -f $(TARGET_DIR)/usr/bin/fwupdate.sh
	rm -f $(TARGET_DIR)/usr/bin/fpga-update.sh
	#rm -f $(TARGET_DIR)/etc/ssl_cert.pem
	rm -rf $(TARGET_DIR)/var/www
endef

$(eval $(generic-package))
