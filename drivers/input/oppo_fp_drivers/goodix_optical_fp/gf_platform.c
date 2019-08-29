/************************************************************************************
 ** File: - SDM660.LA.1.0\android\vendor\oppo_app\fingerprints_hal\drivers\goodix_fp\gf_platform.c
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
 **  Ran.Chen      2017/11/20      add vreg_step for goodix_fp
 **  Ran.Chen      2018/11/26      add for sdm855, used pwr_gpio
 **  Ran.Chen      2018/11/30      modify for powe_on/off for SDM855
 **  Ran.Chen      2018/12/15      modify for powe_on/off for SDM855
 ************************************************************************************/
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>
#include <soc/oppo/oppo_project.h>

#include "gf_spi.h"

#if defined(USE_SPI_BUS)
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>
#elif defined(USE_PLATFORM_BUS)
#include <linux/platform_device.h>
#endif

#ifndef USED_GPIO_PWR
struct vreg_config {
    char *name;
    unsigned long vmin;
    unsigned long vmax;
    int ua_load;
};

static const struct vreg_config const vreg_conf[] = {
    { "ldo5", 2960000UL, 2960000UL, 150000, },
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
	struct device *dev = &gf_dev->spi->dev;
	struct device_node *np = dev->of_node;

	gf_dev->reset_gpio = of_get_named_gpio(np, "goodix,gpio_reset", 0);
	if (gf_dev->reset_gpio < 0) {
		pr_err("falied to get reset gpio!\n");
		return gf_dev->reset_gpio;
	}

	rc = devm_gpio_request(dev, gf_dev->reset_gpio, "goodix_reset");
	if (rc) {
		pr_err("failed to request reset gpio, rc = %d\n", rc);
		goto err_reset;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);

	gf_dev->irq_gpio = of_get_named_gpio(np, "goodix,gpio_irq", 0);
	if (gf_dev->irq_gpio < 0) {
		pr_err("falied to get irq gpio!\n");
		return gf_dev->irq_gpio;
	}

	rc = devm_gpio_request(dev, gf_dev->irq_gpio, "goodix_irq");
	if (rc) {
		pr_err("failed to request irq gpio, rc = %d\n", rc);
		goto err_irq;
	}
	gpio_direction_input(gf_dev->irq_gpio);

#if defined(USED_GPIO_PWR)

    gf_dev->pwr_gpio = of_get_named_gpio(np, "goodix,goodix_pwr", 0);
        pr_err("end of_get_named_gpio  goodix_pwr!\n");
    if (gf_dev->pwr_gpio < 0) {
        pr_err("falied to get goodix_pwr gpio!\n");
        return gf_dev->pwr_gpio;
    }

    rc = devm_gpio_request(dev, gf_dev->pwr_gpio, "goodix_pwr");
    if (rc) {
        pr_err("failed to request goodix_pwr gpio, rc = %d\n", rc);
        goto err_pwr;
    }
    gpio_direction_output(gf_dev->pwr_gpio, 0);
    pr_err("set goodix_pwr output 0 \n");
#elif defined(PROJECT_18383)
    if (is_project(OPPO_18383)) {
        gf_dev->pwr_gpio = of_get_named_gpio(np, "goodix,goodix_pwr", 0);
            pr_err("end of_get_named_gpio  goodix_pwr!\n");
        if (gf_dev->pwr_gpio < 0) {
            pr_err("falied to get goodix_pwr gpio!\n");
            return gf_dev->pwr_gpio;
        }

        rc = devm_gpio_request(dev, gf_dev->pwr_gpio, "goodix_pwr");
        if (rc) {
            pr_err("failed to request goodix_pwr gpio, rc = %d\n", rc);
            devm_gpio_free(dev, gf_dev->pwr_gpio);
        }
        gpio_direction_output(gf_dev->pwr_gpio, 0);
        pr_err("set goodix_pwr output 0 \n");
    }
#endif

pr_err("end gf_parse_dts !\n");

#if defined(USED_GPIO_PWR)
err_pwr:
    devm_gpio_free(dev, gf_dev->pwr_gpio);
#endif

err_irq:
	devm_gpio_free(dev, gf_dev->reset_gpio);
err_reset:
	return rc;
}

void gf_cleanup(struct gf_dev *gf_dev)
{
	pr_info("[info] %s\n",__func__);
	if (gpio_is_valid(gf_dev->irq_gpio))
	{
		gpio_free(gf_dev->irq_gpio);
		pr_info("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio))
	{
		gpio_free(gf_dev->reset_gpio);
		pr_info("remove reset_gpio success\n");
	}
#if defined(USED_GPIO_PWR)
    if (gpio_is_valid(gf_dev->pwr_gpio))
    {
        gpio_free(gf_dev->pwr_gpio);
        pr_info("remove pwr_gpio success\n");
    }
#elif defined(PROJECT_18383)
    if (is_project(OPPO_18383)) {
        if (gpio_is_valid(gf_dev->pwr_gpio))
        {
            gpio_free(gf_dev->pwr_gpio);
            pr_info("remove pwr_gpio success\n");
        }
    }
#endif
}

int gf_power_on(struct gf_dev* gf_dev)
{
    int rc = 0;
    pr_info("---- power on ok ----\n");
#if defined(USED_GPIO_PWR)
    gpio_set_value(gf_dev->pwr_gpio, 1);
    pr_info("set pwe_gpio 1\n");
#elif defined(PROJECT_18383)
    if (is_project(OPPO_18383)) {
        gpio_set_value(gf_dev->pwr_gpio, 1);
        pr_info("set pwe_gpio 1\n");
    } else {
        rc = vreg_setup(gf_dev, "ldo5", true);
    }
#else 
	rc = vreg_setup(gf_dev, "ldo5", true);
#endif
    msleep(30);
    return rc;
}

int gf_power_off(struct gf_dev* gf_dev)
{
    int rc = 0;

    pr_info("---- power off ----\n");
#if defined(USED_GPIO_PWR)
    gpio_set_value(gf_dev->pwr_gpio, 0);
    pr_info("set pwe_gpio 0\n");
#elif defined(PROJECT_18383)
    if (is_project(OPPO_18383)) {
        gpio_set_value(gf_dev->pwr_gpio, 0);
        pr_info("set pwe_gpio 0\n");
    } else {
        rc = vreg_setup(gf_dev, "ldo5", false);
    }
#else
	rc = vreg_setup(gf_dev, "ldo5", false);
#endif
    msleep(30);
    return rc;
}

int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if(gf_dev == NULL) {
		pr_info("Input buff is NULL.\n");
		return -1;
	}
	//gpio_direction_output(gf_dev->reset_gpio, 1);
	gpio_set_value(gf_dev->reset_gpio, 0);
	mdelay(5);
	gpio_set_value(gf_dev->reset_gpio, 1);
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

