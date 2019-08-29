/************************************************************************************
** File: - MSM8953_LA_1_0\android\kernel\include\soc\oppo\oppo_fp_common.h
** VENDOR_EDIT
** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd
**
** Description:
**          fingerprint compatibility configuration
**
** Version: 1.0
** Date created: 18:03:11, 23/07/2016
** Author: Ziqing.guo@Prd.BaseDrv
**
** --------------------------- Revision History: --------------------------------
**  <author>                 <data>                        <desc>
**  Ziqing.guo   2016/07/23           create the file
**  Hongdao.yu   2017/03/09           add FP_FPC gpio_id0 fp_id0 to adapt for 16037 15103
**  Hongdao.yu   2017/03/22           remake ID match code
**  Ziqing.guo   2017/04/29           add for displaying secure boot switch
**  Bin.Li       2017/12/12           remove secure part
**  Ziqing.guo   2019/02/12           modify FP_ID_MAX_LENGTH (20 -> 60)
**  Qing.Guan    2019/04/01           add for egis optical fp
************************************************************************************/

#ifndef _OPPO_FP_COMMON_H_
#define _OPPO_FP_COMMON_H_

#include <linux/platform_device.h>

#define FP_ID_MAX_LENGTH                60 /*the length of /proc/fp_id should less than FP_ID_MAX_LENGTH !!!*/
#define ENGINEER_MENU_SELECT_MAXLENTH   20
#define FP_ID_SUFFIX_MAX_LENGTH         30 /*the length of FP ID SUFFIX !!!*/

typedef enum {
        FP_FPC_1022,
        FP_FPC_1023,
        FP_FPC_1140,
        FP_FPC_1260,
        FP_FPC_1270,
        FP_GOODIX_5658,
        FP_FPC_1511,
        FP_GOODIX_3268,
        FP_GOODIX_5288,
        FP_GOODIX_5298,
        FP_GOODIX_5228,
        FP_GOODIX_OPTICAL_95,
        FP_GOODIX_5298_GLASS,
        FP_SILEAD_OPTICAL_70,
        FP_FPC_1023_GLASS,
	FP_EGIS_OPTICAL_ET713,
        FP_UNKNOWN,
} fp_vendor_t;

enum {
        FP_OK,
        FP_ERROR_GPIO,
        FP_ERROR_GENERAL,
};

struct fp_data {
        struct device *dev;
        int gpio_id0;
        int gpio_id1;
        int gpio_id2;
        int gpio_id3;
        int fp_id0;
        int fp_id1;
        int fp_id2;
        int fp_id3;
        fp_vendor_t fpsensor_type;
};

struct fp_underscreen_info {
    uint8_t touch_state;
    uint8_t area_rate;
    uint16_t x;
    uint16_t y;
};

typedef struct fp_module_gpio_config_info {
        int gpio_id_config_list[3];  /*gpio_id_config_list[0]-->gpio ID1, and so on; if a ID is not used, then value == -1*/
        int fp_vendor_chip;
        char fp_chip_module[FP_ID_MAX_LENGTH];
        char engineermode_menu_config[ENGINEER_MENU_SELECT_MAXLENTH];  /*it depends on macro ENGINEER_MENU_XXX in oppo_fp_common.c*/
}fp_module_config_t;

fp_vendor_t get_fpsensor_type(void);

#endif  /*_OPPO_FP_COMMON_H_*/
