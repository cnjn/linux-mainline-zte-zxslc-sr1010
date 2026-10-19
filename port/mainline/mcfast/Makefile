#
# mcfast: tc BPF fast path for routed IPv4 multicast
#
# This is free software, licensed under the GNU General Public License v2.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=mcfast
PKG_RELEASE:=1

PKG_LICENSE:=GPL-2.0-only
PKG_BUILD_DEPENDS:=bpf-headers
PKG_FLAGS:=nonshared

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/bpf.mk

define Package/mcfast
  SECTION:=net
  CATEGORY:=Network
  SUBMENU:=Routing and Redirection
  TITLE:=eBPF fast path for routed IPv4 and IPv6 multicast
  DEPENDS:=+libbpf $(BPF_DEPENDS)
endef

define Package/mcfast/description
  Forwards IPv4 and IPv6 multicast streams that the kernel multicast routing
  tables (as set up by omcproxy, igmpproxy or smcroute) have resolved directly
  at tc ingress of the upstream device, bypassing conntrack, netfilter and
  ipmr/ip6mr. Bridges still apply IGMP and MLD snooping.
endef

define Package/mcfast/conffiles
/etc/config/mcfast
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(call CompileBPF,$(PKG_BUILD_DIR)/mcfast-bpf.c)
	$(TARGET_CC) $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -Wall \
		-o $(PKG_BUILD_DIR)/mcfast $(PKG_BUILD_DIR)/mcfast.c \
		$(TARGET_LDFLAGS) -lbpf
endef

define Package/mcfast/install
	$(INSTALL_DIR) $(1)/lib/bpf $(1)/usr/sbin $(1)/etc/init.d $(1)/etc/config
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/mcfast-bpf.o $(1)/lib/bpf/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/mcfast $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/mcfast.init $(1)/etc/init.d/mcfast
	$(INSTALL_CONF) ./files/mcfast.config $(1)/etc/config/mcfast
endef

$(eval $(call BuildPackage,mcfast))
