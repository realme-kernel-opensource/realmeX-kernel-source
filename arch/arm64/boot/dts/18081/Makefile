ifeq ($(CONFIG_BUILD_ARM64_DT_OVERLAY),y)
	dtbo-$(CONFIG_ARCH_SDM670) += \
		sdm670-pm660a-mtp-overlay.dtbo

sdm670-pm660a-mtp-overlay.dtbo-base := sdm670.dtb
else
dtb-$(CONFIG_ARCH_SDM670) += 	sdm670-mtp.dtb
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
