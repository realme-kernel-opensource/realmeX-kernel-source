ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM670) += \
		sdm710-pm660a-mtp-overlay.dtbo

sdm710-pm660a-mtp-overlay.dtbo-base := sdm710.dtb
else
dtb-$(CONFIG_ARCH_SDM670) += 	sdm710.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
