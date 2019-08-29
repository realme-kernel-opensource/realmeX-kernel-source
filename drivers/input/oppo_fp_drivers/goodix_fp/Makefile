DEFS = -DCONFIG_OPPO_FINGERPRINT_KERNEL_VER=$(VERSION)
ccflags-y += $(DEFS)

ifeq ($(CONFIG_OPPO_FINGERPRINT_QCOM),y)
obj-y  += gf_spi.o gf_platform.o gf_netlink.o
else ifeq ($(CONFIG_OPPO_FINGERPRINT_MTK),y)
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
include $(srctree)/drivers/misc/mediatek/gud/Makefile.include

ifneq ($(filter 17331, $(shell echo $(TARGET_PRODUCT) | sed -e 's/full_oppo6771_//g')),)
DEFS_PROJ = -DCONFIG_MT6771_17331
ccflags-y += $(DEFS_PROJ)
endif

obj-y  += gf_tee.o
gf_tee-y := gf_spi_tee.o gf_platform_mtk.o
endif