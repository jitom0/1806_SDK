include $(TOPDIR)/rules.mk

PKG_NAME:=sbleupdate
PKG_VERSION:=1.0
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/sbleupdate
	SECTION:=utils
	CATEGORY:=Sealan
	TITLE:=SBLE OTA update utility
	DEFAULT:=n
endef

define Package/sbleupdate/description
	Serial OTA update utility for SBLE firmware images.
endef

define Build/Prepare
	$(RM) -r $(PKG_BUILD_DIR)
	$(INSTALL_DIR) $(PKG_BUILD_DIR)
	$(CP) ./src/main.c $(PKG_BUILD_DIR)/
	$(CP) ./src/ota_protocol.c $(PKG_BUILD_DIR)/
	$(CP) ./src/ota_protocol.h $(PKG_BUILD_DIR)/
	$(CP) ./src/serial_port.c $(PKG_BUILD_DIR)/
	$(CP) ./src/serial_port.h $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -Wall -Wextra \
		-o $(PKG_BUILD_DIR)/sbleupdate \
		$(PKG_BUILD_DIR)/main.c \
		$(PKG_BUILD_DIR)/ota_protocol.c \
		$(PKG_BUILD_DIR)/serial_port.c \
		$(TARGET_LDFLAGS)
endef

define Package/sbleupdate/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/sbleupdate $(1)/usr/bin/
endef

$(eval $(call BuildPackage,sbleupdate))
