/************************************************************************************
 ** File: - \android\vendor\oppo_app\fingerprints_hal\drivers\goodix_fp\gf_platform.c
 ** VENDOR_EDIT
 ** Copyright (C), 2008-2017, OPPO Mobile Comm Corp., Ltd
 **
 ** Description:
 **      goodix fingerprint kernel device driver
 **
 ** Version: 1.0
 ** Date created: 10:10:11,11/24/2017
 ** Author: Chen.ran@Prd.BaseDrv
 ** TAG: BSP.Fingerprint.Basic
 **
 ** --------------------------- Revision History: --------------------------------
 **  <author>        <data>          <desc>
 **  Dongnan.Wu     2019/02/23      init the platform driver for goodix device
 **  Dongnan.Wu     2019/03/18      modify for goodix fp spi drive strength
 **  Bangxiong.Wu   2019/04/05      modify for correcting time sequence during boot
************************************************************************************/

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <soc/oppo/oppo_project.h>

#include "gf_spi_tee.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>

//static struct pinctrl *gf_irq_pinctrl = NULL;
//static struct pinctrl_state *gf_irq_no_pull = NULL;

#ifndef USED_GPIO_PWR
struct vreg_config {
    char *name;
    unsigned long vmin;
    unsigned long vmax;
    int ua_load;
};

static const struct vreg_config const vreg_conf[] = {
    { "avdd", 3000000, 3000000, 150000, },
};

static int vreg_setup(struct gf_dev *goodix_fp, const char *name,
    bool enable)
{
    size_t i;
    int rc;
    struct regulator *vreg;
    struct device *dev = &goodix_fp->spi->dev;
    if (NULL == name) {
        pr_err("name is NULL\n");
        return -EINVAL;
    }
	pr_err("Regulator %s vreg_setup,enable=%d \n", name, enable);
    for (i = 0; i < ARRAY_SIZE(goodix_fp->vreg); i++) {
        const char *n = vreg_conf[i].name;
        if (!strncmp(n, name, strlen(n)))
            goto found;
    }
    pr_err("Regulator %s not found\n", name);
    return -EINVAL;
found:
    vreg = goodix_fp->vreg[i];
    if (enable) {
        if (!vreg) {
            vreg = regulator_get(dev, name);
            if (IS_ERR(vreg)) {
                pr_err("Unable to get  %s\n", name);
                return PTR_ERR(vreg);
            }
        }
        if (regulator_count_voltages(vreg) > 0) {
            rc = regulator_set_voltage(vreg, vreg_conf[i].vmin,
                    vreg_conf[i].vmax);
            if (rc)
                pr_err("Unable to set voltage on %s, %d\n",
                    name, rc);
        }
        rc = regulator_set_load(vreg, vreg_conf[i].ua_load);
        if (rc < 0)
            pr_err("Unable to set current on %s, %d\n",
                    name, rc);
        rc = regulator_enable(vreg);
        if (rc) {
            pr_err("error enabling %s: %d\n", name, rc);
            regulator_put(vreg);
            vreg = NULL;
        }
        goodix_fp->vreg[i] = vreg;
    } else {
        if (vreg) {
            if (regulator_is_enabled(vreg)) {
                regulator_disable(vreg);
                pr_err("disabled %s\n", name);
            }
            regulator_put(vreg);
            goodix_fp->vreg[i] = NULL;
        }
		pr_err("disable vreg is null \n");
        rc = 0;
    }
    return rc;
}
#endif

int gf_parse_dts(struct gf_dev* gf_dev)
{
	int rc = 0;
	struct device_node *node = NULL;
	struct platform_device *pdev = NULL;
        gf_dev->cs_gpio_set = false;
        gf_dev->pinctrl = NULL;
        gf_dev->pstate_spi_6mA = NULL;
        gf_dev->pstate_default = NULL;
        gf_dev->pstate_cs_func = NULL;

	node = of_find_compatible_node(NULL, NULL, "goodix,goodix_fp");
	if (node) {
		pdev = of_find_device_by_node(node);
		if (pdev == NULL) {
            pr_err("[err] %s can not find device by node \n", __func__);
            return -1;
        }
    } else {
        pr_err("[err] %s can not find compatible node \n", __func__);
        return -1;
    }

         /*get clk pinctrl resource*/
        gf_dev->pinctrl = devm_pinctrl_get(&pdev->dev);
        if (IS_ERR(gf_dev->pinctrl)) {
                dev_err(&pdev->dev, "can not get the gf pinctrl");
                return PTR_ERR(gf_dev->pinctrl);
        }
        gf_dev->pstate_spi_6mA  = pinctrl_lookup_state(gf_dev->pinctrl, "gf_spi_drive_6mA");
        if (IS_ERR(gf_dev->pstate_spi_6mA)) {
                dev_err(&pdev->dev, "Can't find gf_spi_drive_6mA pinctrl state\n");
                return PTR_ERR(gf_dev->pstate_spi_6mA);
        }
        pinctrl_select_state(gf_dev->pinctrl, gf_dev->pstate_spi_6mA);

        gf_dev->pstate_default = pinctrl_lookup_state(gf_dev->pinctrl, "default");
        if (IS_ERR(gf_dev->pstate_default)) {
                dev_err(&pdev->dev, "Can't find default pinctrl state\n");
                return PTR_ERR(gf_dev->pstate_default);
        }
        pinctrl_select_state(gf_dev->pinctrl, gf_dev->pstate_default);

        gf_dev->pstate_cs_func = pinctrl_lookup_state(gf_dev->pinctrl, "gf_cs_func");
        if (IS_ERR(gf_dev->pstate_cs_func)) {
                dev_err(&pdev->dev, "Can't find gf_cs_func pinctrl state\n");
                return PTR_ERR(gf_dev->pstate_cs_func);
        }

	/*get reset resource*/
	gf_dev->reset_gpio = of_get_named_gpio(pdev->dev.of_node, "goodix,gpio_reset", 0);
	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		pr_info("RESET GPIO is invalid.\n");
		return -1;
	}

	rc = gpio_request(gf_dev->reset_gpio, "goodix_reset");
	if (rc) {
		dev_err(&gf_dev->spi->dev, "Failed to request RESET GPIO. rc = %d\n", rc);
		return -1;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);
	msleep(3);

        /*get cs resource*/
        gf_dev->cs_gpio = of_get_named_gpio(pdev->dev.of_node, "goodix,gpio_cs", 0);
        if (!gpio_is_valid(gf_dev->cs_gpio)) {
                pr_info("CS GPIO is invalid.\n");
                return -1;
        }
        rc = gpio_request(gf_dev->cs_gpio, "goodix_cs");
        if (rc) {
                dev_err(&gf_dev->spi->dev, "Failed to request CS GPIO. rc = %d\n", rc);
                return -1;
        }
        gpio_direction_output(gf_dev->cs_gpio, 0);
        gf_dev->cs_gpio_set = true;

	/*get irq resourece*/
	gf_dev->irq_gpio = of_get_named_gpio(pdev->dev.of_node, "goodix,gpio_irq", 0);
	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		pr_info("IRQ GPIO is invalid.\n");
		return -1;
	}

	rc = gpio_request(gf_dev->irq_gpio, "goodix_irq");
	if (rc) {
		dev_err(&gf_dev->spi->dev, "Failed to request IRQ GPIO. rc = %d\n", rc);
		return -1;
	}
	gpio_direction_input(gf_dev->irq_gpio);

	return 0;
}

void gf_cleanup(struct gf_dev* gf_dev)
{
	pr_info("[info] %s\n",__func__);
	if (gpio_is_valid(gf_dev->irq_gpio))
	{
		gpio_free(gf_dev->irq_gpio);
		pr_info("remove irq_gpio success\n");
	}
        if (gpio_is_valid(gf_dev->cs_gpio)) {
                gpio_free(gf_dev->cs_gpio);
                pr_info("remove cs_gpio success\n");
        }
	if (gpio_is_valid(gf_dev->reset_gpio))
	{
		gpio_free(gf_dev->reset_gpio);
		pr_info("remove reset_gpio success\n");
	}
}

int gf_power_on(struct gf_dev* gf_dev)
{
	int rc = 0;

#ifdef CONFIG_MT6771_17331
	if (get_project() != 17197) {
		gpio_set_value(gf_dev->ldo_gpio, 1);
		msleep(10);
	}
#endif
	rc = vreg_setup(gf_dev, "avdd", true);
        if (rc) {
                pr_err("%s power on fail \n", __func__);
                return rc;
        }

        rc = vreg_setup(gf_dev, "avdd", false);
        if (rc) {
                pr_err("%s power off fail \n", __func__);
                return rc;
        }
        msleep(30);

        rc = vreg_setup(gf_dev, "avdd", true);
        if (rc) {
                pr_err("%s power on fail \n", __func__);
                return rc;
        }
        pr_info("%s ---- power on ok ----\n", __func__);

        return rc;
}

int gf_power_off(struct gf_dev* gf_dev)
{
	int rc = 0;
	rc = vreg_setup(gf_dev, "avdd", false);
	if (rc) {
		pr_err("%s power off fail \n", __func__);
		return rc;
	}
	pr_info("---- power off ----\n");
	return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if(gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -1;
	}

	gpio_set_value(gf_dev->reset_gpio, 0);
	mdelay(20);
	gpio_set_value(gf_dev->reset_gpio, 1);
        if (gf_dev->cs_gpio_set) {
                pr_info("---- pull CS up and set CS from gpio to func ----");
                gpio_set_value(gf_dev->cs_gpio, 1);
                pinctrl_select_state(gf_dev->pinctrl, gf_dev->pstate_cs_func);
                gf_dev->cs_gpio_set = false;
        }
	mdelay(delay_ms);
	return 0;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if(gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -1;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}

