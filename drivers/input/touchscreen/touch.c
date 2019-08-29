/***************************************************
 * File:touch.c
 * VENDOR_EDIT
 * Copyright (c)  2008- 2030  Oppo Mobile communication Corp.ltd.
 * Description:
 *             tp dev
 * Version:1.0:
 * Date created:2016/09/02
 * Author: hao.wang@Bsp.Driver
 * TAG: BSP.TP.Init
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include "oppo_touchscreen/tp_devices.h"
#include "oppo_touchscreen/touchpanel_common.h"
#include <soc/oppo/oppo_project.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>

#define MAX_LIMIT_DATA_LENGTH         100

/*if can not compile success, please update vendor/oppo_touchsreen*/
struct tp_dev_name tp_dev_names[] = {
     {TP_OFILM, "OFILM"},
     {TP_BIEL, "BIEL"},
     {TP_TRULY, "TRULY"},
     {TP_BOE, "BOE"},
     {TP_G2Y, "G2Y"},
     {TP_TPK, "TPK"},
     {TP_JDI, "JDI"},
     {TP_TIANMA, "TIANMA"},
     {TP_SAMSUNG, "SAMSUNG"},
     {TP_DSJM, "DSJM"},
     {TP_BOE_B8, "BOEB8"},
     {TP_UNKNOWN, "UNKNOWN"},
};

#define GET_TP_DEV_NAME(tp_type) ((tp_dev_names[tp_type].type == (tp_type))?tp_dev_names[tp_type].name:"UNMATCH")

int tp_util_get_vendor(struct hw_resource *hw_res, struct panel_info *panel_data)
{
    char* vendor;
    int prj_id = 0;

    panel_data->test_limit_name = kzalloc(MAX_LIMIT_DATA_LENGTH, GFP_KERNEL);
    if (panel_data->test_limit_name == NULL) {
        pr_err("[TP]panel_data.test_limit_name kzalloc error\n");
    }

    if (is_project(OPPO_18081) || is_project(OPPO_18085) || is_project(OPPO_18181)) {
        panel_data->tp_type = TP_SAMSUNG;
        prj_id = OPPO_18081;
    } else if (is_project(OPPO_18097) || is_project(OPPO_18099)) {
        panel_data->tp_type = TP_SAMSUNG;
        prj_id = OPPO_18097;
    } else if (is_project(OPPO_18041)) {
        panel_data->tp_type = TP_SAMSUNG;
        if (get_PCB_Version() == HW_VERSION__10) {
            pr_info("18041 T0 use 18097 TP FW\n");
            prj_id = OPPO_18097;
        } else {
            prj_id = OPPO_18041;
        }
    } else if (is_project(OPPO_18383)) {
        panel_data->tp_type = TP_SAMSUNG;
        prj_id = OPPO_18383;
    }
    if (panel_data->tp_type == TP_UNKNOWN) {
        pr_err("[TP]%s type is unknown\n", __func__);
        return 0;
    }

    vendor = GET_TP_DEV_NAME(panel_data->tp_type);

    strcpy(panel_data->manufacture_info.manufacture, vendor);
    snprintf(panel_data->fw_name, MAX_FW_NAME_LENGTH,
            "tp/%d/FW_%s_%s.img",
            prj_id, panel_data->chip_name, vendor);

    if (panel_data->test_limit_name) {
        snprintf(panel_data->test_limit_name, MAX_LIMIT_DATA_LENGTH,
            "tp/%d/LIMIT_%s_%s.img",
            prj_id, panel_data->chip_name, vendor);
    }
    panel_data->manufacture_info.fw_path = panel_data->fw_name;

    pr_info("[TP]vendor:%s fw:%s limit:%s\n",
        vendor,
        panel_data->fw_name,
        panel_data->test_limit_name==NULL?"NO Limit":panel_data->test_limit_name);
    return 0;
}

int reconfig_power_control(struct touchpanel_data *ts)
{
    int ret = 0;
    if ((is_project(OPPO_18097) || is_project(OPPO_18099))) {
        if (get_PCB_Version() == HW_VERSION__10) {
            if (gpio_is_valid(ts->hw_res.reset_gpio)) {
                gpio_free(ts->hw_res.reset_gpio);
            } else {
                pr_info("ts->reset-gpio not specified\n");
            }

            ts->hw_res.reset_gpio = of_get_named_gpio(ts->dev->of_node, "reset_t0-gpio", 0);
            if (gpio_is_valid(ts->hw_res.reset_gpio)) {
                ret = gpio_request(ts->hw_res.reset_gpio, "reset-gpio");
                if (ret) {
                    pr_info("unable to request gpio [%d]\n", ts->hw_res.reset_gpio);
                } else {
                    pr_info("ts->reset-gpio not specified\n");
                }
            }
        } else {
            if (!IS_ERR_OR_NULL(ts->hw_res.vdd_2v8)) {
                //ret = regulator_disable(ts->hw_res.vdd_2v8);
                regulator_put(ts->hw_res.vdd_2v8);
                ts->hw_res.vdd_2v8 = NULL;
            }
        }
    }
    if (is_project(OPPO_18041)) {
        if (get_PCB_Version() == HW_VERSION__10) {
            ts->hw_res.TX_NUM = 16;
            ts->hw_res.RX_NUM = 33;
            pr_info("18041 T0 use TX,RX=[%d],[%d]\n", ts->hw_res.TX_NUM, ts->hw_res.RX_NUM);
        }
    }

    return ret;
}