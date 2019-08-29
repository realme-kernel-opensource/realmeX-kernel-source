ccflags-y += -DBSP_SIL_PLAT_QCOM
ifneq ($(filter MSM_18115 MSM_18501 MSM_18503 MSM_18119 MSM_19061 MSM_19361, $(OPPO_TARGET_DEVICE)),)
ccflags-y += -DUSED_GPIO_PWR
ccflags-y += -DCONFIG_MSM_855
endif
obj-y	+= silead_fp_platform.o