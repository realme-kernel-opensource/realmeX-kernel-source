/************************************************************************************
** File: - drivers\input\fpc_tee\fpc_irq.c
** VENDOR_EDIT
** Copyright (C), 2008-2016, OPPO Mobile Comm Corp., Ltd
**
** Description:
**      fpc fingerprint kernel device driver
**
** Version: 1.0
** Date created: 15:03:11,22/11/2017
** Author: LiBin@BSP.Fingerprint.Basic
** TAG: BSP.Fingerprint.Basic
**
** --------------------------- Revision History: --------------------------------
**    <author>     <data>        <desc>
**    LiBin        2017/11/22    create the file
**    LiBin        2018/01/05    Modify for reset sequence
**    Ran.Chen     2018/06/26    add for 1023_2060_GLASS
**    Long.Liu     2018/09/29    add for 18531 fpc1023
**    Long.Liu     2018/11/19    modify 18531 static test
**    Long.Liu     2018/11/23    add for 18151 fpc1023
**    Yang.Tan     2018/11/26    add for 18531 fpc1511
**    Long.Liu     2019/01/03    add for 18161 fpc1511
************************************************************************************/

#include <linux/version.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
//#include <linux/irq.h>

#define DEBUG
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
#include <linux/wakelock.h>
#else
#include <linux/pm_wakeup.h>
#endif
#include <linux/spi/spi.h>
#include "../include/oppo_fp_common.h"

#define FPC_IRQ_DEV_NAME    "fpc_irq"

/* Uncomment if DeviceTree should be used */

//#include <mtk_spi_hal.h>
#include <linux/platform_data/spi-mt65xx.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
#include <mt-plat/mtk_gpio.h>
#include <linux/irqchip/mtk-eic.h>
#endif
#include <linux/of_address.h>
#include <linux/of_device.h>

// Platform specific


#define   FPC1020_RESET_LOW_US                              1000
#define   FPC1020_RESET_HIGH1_US                            100
#define   FPC1020_RESET_HIGH2_US                            1250
#define   FPC_TTW_HOLD_TIME                                 1000
#define   FPC_IRQ_WAKELOCK_TIMEOUT                          500
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 5, 0))
#define   FPC_WL_WAKELOCK_TIMEOUT                          0
#endif

#define   WAKELOCK_DISABLE                                  0
#define   WAKELOCK_ENABLE                                   1
#define   WAKELOCK_TIMEOUT_ENABLE                           2
#define   WAKELOCK_TIMEOUT_DISABLE                          3
struct mtk_spi {
        void __iomem                                        *base;
        void __iomem                                        *peri_regs;
        u32                                                 state;
        int                                                 pad_num;
        u32                                                 *pad_sel;
        struct clk                                          *parent_clk, *sel_clk, *spi_clk;
        struct spi_transfer                                 *cur_transfer;
        u32                                                 xfer_len;
        struct scatterlist                                  *tx_sgl, *rx_sgl;
        u32                                                 tx_sgl_len, rx_sgl_len;
        const struct mtk_spi_compatible                     *dev_comp;
        u32                                                 dram_8gb_offset;
};

struct clk *globle_spi_clk;
struct mtk_spi *fpc_ms;

typedef struct {
        struct spi_device                                   *spi;
        struct class                                        *class;
        struct device                                       *device;
        dev_t                                               devno;
        u8                                                  *huge_buffer;
        size_t                                              huge_buffer_size;
        struct input_dev                                    *input_dev;
} fpc1020_data_t;

struct vreg_config {
        char *name;
        unsigned long vmin;
        unsigned long vmax;
        int ua_load;
};

#if CONFIG_OPPO_FINGERPRINT_PROJCT == 18151
static const struct vreg_config const vreg_conf[] = {
        { "vdd_io", 3000000UL, 3000000UL, 6000, },
};
#else
static const struct vreg_config const vreg_conf[] = {
        { "vdd_io", 1800000UL, 1800000UL, 6000, },
};
#endif

struct fpc1020_data {
        struct device *dev;
        struct platform_device *pldev;
        int irq_gpio;
        int rst_gpio;
#if CONFIG_OPPO_FINGERPRINT_PROJCT == 18531 || CONFIG_OPPO_FINGERPRINT_PROJCT == 18161
    int vdd_en_gpio;
#endif
        struct input_dev *idev;
        int irq_num;
        struct mutex lock;
        bool prepared;

#ifdef VENDOR_EDIT
        //LiBin@BSP.Fingerprint.Basic, 2016/10/13, modify for enable/disable irq
        int irq_enabled;
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
        struct wake_lock                                ttw_wl;
        struct wake_lock                                fpc_wl;
        struct wake_lock                                fpc_irq_wl;
#else
        struct wakeup_source                                ttw_wl;
        struct wakeup_source                                fpc_wl;
        struct wakeup_source                                fpc_irq_wl;
#endif
        struct regulator                                *vreg[ARRAY_SIZE(vreg_conf)];
};

static int fpc1020_request_named_gpio(struct fpc1020_data *fpc1020,
                const char *label, int *gpio)
{
        struct device *dev = fpc1020->dev;
        struct device_node *np = dev->of_node;
        int rc = of_get_named_gpio(np, label, 0);
        if (rc < 0) {
                dev_err(dev, "failed to get '%s'\n", label);
                return rc;
        }

        *gpio = rc;
        rc = devm_gpio_request(dev, *gpio, label);
        if (rc) {
                dev_err(dev, "failed to request gpio %d\n", *gpio);
                return rc;
        }

        /*dev_info(dev, "%s - gpio: %d\n", label, *gpio);*/
        return 0;
}

static int vreg_setup(struct fpc1020_data *fpc1020, const char *name,
                bool enable)
{
        size_t i;
        int rc;
        struct regulator *vreg;
        struct device *dev = fpc1020->dev;

        for (i = 0; i < ARRAY_SIZE(fpc1020->vreg); i++) {
                const char *n = vreg_conf[i].name;
                if (!strncmp(n, name, strlen(n))) {
                        goto found;
                }
        }
        dev_err(dev, "Regulator %s not found\n", name);
        return -EINVAL;

found:
        vreg = fpc1020->vreg[i];
        if (enable) {
                if (!vreg) {
                        vreg = regulator_get(dev, name);
                        if (IS_ERR(vreg)) {
                                dev_err(dev, "Unable to get  %s\n", name);
                                return PTR_ERR(vreg);
                        }
                }
                if (regulator_count_voltages(vreg) > 0) {
                        rc = regulator_set_voltage(vreg, vreg_conf[i].vmin,
                                        vreg_conf[i].vmax);
                        if (rc) {
                                dev_err(dev,
                                                "Unable to set voltage on %s, %d\n",
                                                name, rc);
                        }
                }

                rc = regulator_set_load(vreg, vreg_conf[i].ua_load);

                if (rc < 0) {
                        dev_err(dev, "Unable to set current on %s, %d\n",
                                        name, rc);
                }
                rc = regulator_enable(vreg);
                if (rc) {
                        dev_err(dev, "error enabling %s: %d\n", name, rc);
                        regulator_put(vreg);
                        vreg = NULL;
                }
                fpc1020->vreg[i] = vreg;
        } else {
                if (vreg) {
                        if (regulator_is_enabled(vreg)) {
                                regulator_disable(vreg);
                                dev_dbg(dev, "disabled %s\n", name);
                        }
                        regulator_put(vreg);
                        fpc1020->vreg[i] = NULL;
                }
                rc = 0;
        }
        return rc;
}

/* -------------------------------------------------------------------------- */
/**
 * sysfs node for controlling whether the driver is allowed
 * to enable SPI clk.
 */

static void fpc_enable_clk(void)
{
        #if !defined(CONFIG_MTK_CLKMGR)
        clk_prepare_enable(globle_spi_clk);
        //pr_err("%s, clk_prepare_enable\n", __func__);
        #else
        enable_clock(MT_CG_PERI_SPI0, "spi");
        //pr_err("%s, enable_clock\n", __func__);
        #endif
        return;
}

static void fpc_disable_clk(void)
{
        #if !defined(CONFIG_MTK_CLKMGR)
        clk_disable_unprepare(globle_spi_clk);
        //pr_err("%s, clk_disable_unprepare\n", __func__);
        #else
        disable_clock(MT_CG_PERI_SPI0, "spi");
        //pr_err("%s, disable_clock\n", __func__);
        #endif
        return;
}

static ssize_t clk_enable_set(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
        struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
        dev_dbg(fpc1020->dev, "why should we set clocks here? i refuse,%s\n", __func__);
        dev_dbg(fpc1020->dev, " buff is %d, %s\n", *buf,__func__);

        //update spi clk
        if(*buf == '1'){
                fpc_enable_clk();
                //dev_info(fpc1020->dev, "%s: enable spi clk\n", __func__);
        }
        if(*buf == '0'){
                fpc_disable_clk();
                //dev_info(fpc1020->dev, "%s: disable spi clk\n", __func__);
        }
        return 1;//set_clks(fpc1020, (*buf == '1')) ? : count;
}

static DEVICE_ATTR(clk_enable, S_IWUSR, NULL, clk_enable_set);

#ifdef VENDOR_EDIT
//LiBin@BSP.Fingerprint.Basic, 2016/10/13, modify for enable/disable irq
static DEFINE_SPINLOCK(fpc1020_lock);

static int fpc1020_enable_irq(struct fpc1020_data *fpc1020, bool enable)
{
        spin_lock_irq(&fpc1020_lock);
        if (enable) {
                if (!fpc1020->irq_enabled) {
                        enable_irq(gpio_to_irq(fpc1020->irq_gpio));
                        enable_irq_wake(gpio_to_irq(fpc1020->irq_gpio));
                        fpc1020->irq_enabled = 1;
                        dev_info(fpc1020->dev, "%s: enable\n", __func__);
                } else {
                        /*dev_info(fpc1020->dev, "%s: no need enable\n", __func__);*/
                }
        } else {
                if (fpc1020->irq_enabled) {
                        disable_irq_wake(gpio_to_irq(fpc1020->irq_gpio));
                        disable_irq_nosync(gpio_to_irq(fpc1020->irq_gpio));
                        fpc1020->irq_enabled = 0;
                        dev_info(fpc1020->dev, "%s: disable\n", __func__);
                } else {
                        /*dev_info(fpc1020->dev, "%s: no need disable\n", __func__);*/
                }
        }
        spin_unlock_irq(&fpc1020_lock);

        return 0;
}
#endif

/**
 * sysf node to check the interrupt status of the sensor, the interrupt
 * handler should perform sysf_notify to allow userland to poll the node.
 */
static ssize_t irq_get(struct device *device,
                struct device_attribute *attribute,
                char *buffer)
{
        struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
        int irq = gpio_get_value(fpc1020->irq_gpio);
        return scnprintf(buffer, PAGE_SIZE, "%i\n", irq);
}


/**
 * writing to the irq node will just drop a printk message
 * and return success, used for latency measurement.
 */
static ssize_t irq_ack(struct device *device,
                struct device_attribute* attribute,
                const char *buffer, size_t count)
{
        struct fpc1020_data* fpc1020 = dev_get_drvdata(device);
        dev_dbg(fpc1020->dev, "%s\n", __func__);
        return count;
}


static ssize_t regulator_enable_set(struct device *dev,
                struct device_attribute *attribute, const char *buffer, size_t count)
{
        int op = 0;
        bool enable = false;
        int rc = 0;
        struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
        if (1 == sscanf(buffer, "%d", &op)) {
                if (op == 1) {
                        enable = true;
                }
                else if (op == 0) {
                        enable = false;
                }
        } else {
                printk("invalid content: '%s', length = %zd\n", buffer, count);
                return -EINVAL;
        }
        rc = vreg_setup(fpc1020, "vdd_io", enable);
        return rc ? rc : count;
}

#ifdef VENDOR_EDIT
//LiBin@BSP.Fingerprint.Basic, 2016/10/13, modify for enable/disable irq
static ssize_t irq_enable_set(struct device *dev,
                struct device_attribute *attribute, const char *buffer, size_t count)
{
        int op = 0;
        bool enable = false;
        int rc = 0;
        struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
        if (1 == sscanf(buffer, "%d", &op)) {
                if (op == 1) {
                        enable = true;
                }
                else if (op == 0) {
                        enable = false;
                }
        } else {
                printk("invalid content: '%s', length = %zd\n", buffer, count);
                return -EINVAL;
        }
        rc = fpc1020_enable_irq(fpc1020,  enable);
        return rc ? rc : count;
}

static ssize_t irq_enable_get(struct device *dev,
                struct device_attribute* attribute,
                char *buffer)
{
        struct fpc1020_data* fpc1020 = dev_get_drvdata(dev);
        return scnprintf(buffer, PAGE_SIZE, "%i\n", fpc1020->irq_enabled);
}
#endif

static ssize_t wakelock_enable_set(struct device *dev,
                struct device_attribute *attribute, const char *buffer, size_t count)
{
        int op = 0;
        struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
        if (1 == sscanf(buffer, "%d", &op)) {
                if (op == WAKELOCK_ENABLE) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
                        wake_lock(&fpc1020->fpc_wl);
#else
                        __pm_wakeup_event(&fpc1020->fpc_wl, FPC_WL_WAKELOCK_TIMEOUT);
#endif
                        /*dev_info(dev, "%s, fpc wake_lock\n", __func__);*/
                } else if (op == WAKELOCK_DISABLE) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
                        wake_unlock(&fpc1020->fpc_wl);
#else
                        __pm_relax(&fpc1020->fpc_wl);
#endif
                        /*dev_info(dev, "%s, fpc wake_unlock\n", __func__);*/
                } else if (op == WAKELOCK_TIMEOUT_ENABLE) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
                        wake_lock_timeout(&fpc1020->ttw_wl, msecs_to_jiffies(FPC_TTW_HOLD_TIME));
#else
                        __pm_wakeup_event(&fpc1020->ttw_wl, FPC_TTW_HOLD_TIME);
#endif
                        /*dev_info(dev, "%s, fpc wake_lock timeout\n", __func__);*/
                } else if (op == WAKELOCK_TIMEOUT_DISABLE) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
                        wake_unlock(&fpc1020->ttw_wl);
#else
                        __pm_relax(&fpc1020->ttw_wl);
#endif
                        /*dev_info(dev, "%s, fpc wake_unlock timeout\n", __func__);*/
                }
        } else {
                printk("invalid content: '%s', length = %zd\n", buffer, count);
                return -EINVAL;
        }

        return count;
}

#if CONFIG_OPPO_FINGERPRINT_PROJCT == 18531 || CONFIG_OPPO_FINGERPRINT_PROJCT == 18161
static ssize_t hardware_reset(struct device *dev, struct device_attribute *attribute, const char *buffer, size_t count)
{
    struct  fpc1020_data *fpc1020 = dev_get_drvdata(dev);
    printk("fpc_interrupt: %s enter\n", __func__);
    gpio_direction_output(fpc1020->vdd_en_gpio, 0);
    gpio_set_value(fpc1020->rst_gpio, 0);
    udelay(FPC1020_RESET_LOW_US);
    gpio_set_value(fpc1020->rst_gpio, 1);
    gpio_direction_output(fpc1020->vdd_en_gpio, 1);
    printk("fpc_interrupt: %s exit\n", __func__);
    return count;
}

static DEVICE_ATTR(irq_unexpected, S_IWUSR, NULL, hardware_reset);
#endif

static DEVICE_ATTR(irq, S_IRUSR | S_IWUSR, irq_get, irq_ack);
static DEVICE_ATTR(regulator_enable, S_IWUSR, NULL, regulator_enable_set);
static DEVICE_ATTR(irq_enable, S_IRUSR | S_IWUSR, irq_enable_get, irq_enable_set);

static DEVICE_ATTR(wakelock_enable, S_IWUSR, NULL, wakelock_enable_set);

static struct attribute *attributes[] = {
        /*
           &dev_attr_hw_reset.attr,
           */
        &dev_attr_irq.attr,
        &dev_attr_regulator_enable.attr,
        &dev_attr_irq_enable.attr,
        &dev_attr_wakelock_enable.attr,
        &dev_attr_clk_enable.attr,
#if CONFIG_OPPO_FINGERPRINT_PROJCT == 18531 || CONFIG_OPPO_FINGERPRINT_PROJCT == 18161
        &dev_attr_irq_unexpected.attr,
#endif
        NULL
};

static const struct attribute_group attribute_group = {
        .attrs = attributes,
};

static irqreturn_t fpc1020_irq_handler(int irq, void *handle)
{
        struct fpc1020_data *fpc1020 = handle;
        dev_info(fpc1020->dev, "%s\n", __func__);

        /* Make sure 'wakeup_enabled' is updated before using it
         ** since this is interrupt context (other thread...) */
        smp_rmb();
        /*
           if (fpc1020->wakeup_enabled ) {
           wake_lock_timeout(&fpc1020->ttw_wl, msecs_to_jiffies(FPC_TTW_HOLD_TIME));
           }
           */

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
        wake_lock_timeout(&fpc1020->fpc_irq_wl, msecs_to_jiffies(FPC_IRQ_WAKELOCK_TIMEOUT));
#else
        __pm_wakeup_event(&fpc1020->fpc_irq_wl, FPC_IRQ_WAKELOCK_TIMEOUT);
#endif

        sysfs_notify(&fpc1020->dev->kobj, NULL, dev_attr_irq.attr.name);

        return IRQ_HANDLED;
}

static int fpc1020_irq_probe(struct platform_device *pldev)
{
        struct device *dev = &pldev->dev;
        int rc = 0;
        int irqf;
        struct device_node *np = dev->of_node;

        struct fpc1020_data *fpc1020 = devm_kzalloc(dev, sizeof(*fpc1020),
                        GFP_KERNEL);
        if (!fpc1020) {
                dev_err(dev, "failed to allocate memory for struct fpc1020_data\n");
                rc = -ENOMEM;
                goto ERR_ALLOC;
        }

        fpc1020->dev = dev;
        dev_info(fpc1020->dev, "-->%s\n", __func__);
        dev_set_drvdata(dev, fpc1020);
        fpc1020->pldev = pldev;

        if (!np) {
                dev_err(dev, "no of node found\n");
                rc = -EINVAL;
                goto ERR_BEFORE_WAKELOCK;
        }

        if ((FP_FPC_1140 != get_fpsensor_type())
                        &&(FP_FPC_1260 != get_fpsensor_type())
                        &&(FP_FPC_1022 != get_fpsensor_type())
                        &&(FP_FPC_1023 != get_fpsensor_type())
                        &&(FP_FPC_1023_GLASS != get_fpsensor_type())
                        &&(FP_FPC_1270 != get_fpsensor_type())
                        &&(FP_FPC_1511 != get_fpsensor_type())) {
                dev_err(dev, "found not fpc sensor\n");
                rc = -EINVAL;
                goto ERR_BEFORE_WAKELOCK;
        }
        dev_info(dev, "found fpc sensor\n");

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
        wake_lock_init(&fpc1020->ttw_wl, WAKE_LOCK_SUSPEND, "fpc_ttw_wl");
        wake_lock_init(&fpc1020->fpc_wl, WAKE_LOCK_SUSPEND, "fpc_wl");
        wake_lock_init(&fpc1020->fpc_irq_wl, WAKE_LOCK_SUSPEND, "fpc_irq_wl");
#else
        wakeup_source_init(&fpc1020->ttw_wl, "fpc_ttw_wl");
        wakeup_source_init(&fpc1020->fpc_wl, "fpc_wl");
        wakeup_source_init(&fpc1020->fpc_irq_wl, "fpc_irq_wl");
#endif

        rc = fpc1020_request_named_gpio(fpc1020, "fpc,irq-gpio",
                        &fpc1020->irq_gpio);
        if (rc) {
                goto ERR_AFTER_WAKELOCK;
        }
        rc = gpio_direction_input(fpc1020->irq_gpio);

        if (rc < 0) {
                dev_err(&fpc1020->pldev->dev,
                "gpio_direction_input failed for INT.\n");
                goto ERR_AFTER_WAKELOCK;
        }

        rc = fpc1020_request_named_gpio(fpc1020, "fpc,reset-gpio",
                &fpc1020->rst_gpio);
        if (rc) {
                goto ERR_AFTER_WAKELOCK;
        }
        /*dev_info(fpc1020->dev, "fpc1020 requested gpio finished \n");*/

        irqf = IRQF_TRIGGER_RISING | IRQF_ONESHOT;
        mutex_init(&fpc1020->lock);
        rc = devm_request_threaded_irq(dev, gpio_to_irq(fpc1020->irq_gpio),
                        NULL, fpc1020_irq_handler, irqf,
                        dev_name(dev), fpc1020);
        if (rc) {
                dev_err(dev, "could not request irq %d\n",
                                gpio_to_irq(fpc1020->irq_gpio));
                goto ERR_AFTER_WAKELOCK;
        }
        /*dev_info(fpc1020->dev, "requested irq %d\n", gpio_to_irq(fpc1020->irq_gpio));*/

        /* Request that the interrupt should be wakeable */
        /*enable_irq_wake( gpio_to_irq( fpc1020->irq_gpio ) );*/

#ifdef VENDOR_EDIT
        /*LiBin@BSP.Fingerprint.Basic, 2016/10/13, modify for enable/disable irq*/
        disable_irq_nosync(gpio_to_irq(fpc1020->irq_gpio));
        fpc1020->irq_enabled = 0;
#endif

        rc = sysfs_create_group(&dev->kobj, &attribute_group);
        if (rc) {
                dev_err(dev, "could not create sysfs\n");
                goto ERR_AFTER_WAKELOCK;
        }

        rc = gpio_direction_output(fpc1020->rst_gpio, 1);

        if (rc) {
                dev_err(fpc1020->dev,
                        "gpio_direction_output (reset) failed.\n");
                goto ERR_AFTER_WAKELOCK;
        }

#if CONFIG_OPPO_FINGERPRINT_PROJCT == 18531 || CONFIG_OPPO_FINGERPRINT_PROJCT == 18161
        /*get ldo resource*/
        rc = fpc1020_request_named_gpio(fpc1020, "fpc,vdd-en",
                    &fpc1020->vdd_en_gpio);
        if (rc) {
                goto ERR_AFTER_WAKELOCK;
        }
        rc = gpio_direction_output(fpc1020->vdd_en_gpio, 1);
        if (rc < 0) {
                dev_err(fpc1020->dev, "gpio_direction_output (vdd_en) failed.\n");
                goto ERR_AFTER_WAKELOCK;
        }
#else
        rc = vreg_setup(fpc1020, "vdd_io", true);

        if (rc) {
                dev_err(fpc1020->dev,
                                "vreg_setup failed.\n");
                goto ERR_AFTER_WAKELOCK;
        }
#endif
        mdelay(2);
        gpio_set_value(fpc1020->rst_gpio, 1);
        udelay(FPC1020_RESET_HIGH2_US);

        dev_info(fpc1020->dev, "%s: ok\n", __func__);
        return 0;

ERR_AFTER_WAKELOCK:
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
        wake_lock_destroy(&fpc1020->ttw_wl);
        wake_lock_destroy(&fpc1020->fpc_wl);
        wake_lock_destroy(&fpc1020->fpc_irq_wl);
#else
        wakeup_source_trash(&fpc1020->ttw_wl);
        wakeup_source_trash(&fpc1020->fpc_wl);
        wakeup_source_trash(&fpc1020->fpc_irq_wl);
#endif
ERR_BEFORE_WAKELOCK:
        dev_err(fpc1020->dev, "%s failed rc = %d\n", __func__, rc);
        devm_kfree(fpc1020->dev, fpc1020);
ERR_ALLOC:
        return rc;
}


static int fpc1020_irq_remove(struct platform_device *pldev)
{
        struct  fpc1020_data *fpc1020 = dev_get_drvdata(&pldev->dev);
        sysfs_remove_group(&pldev->dev.kobj, &attribute_group);
        mutex_destroy(&fpc1020->lock);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0))
        wake_lock_destroy(&fpc1020->ttw_wl);
#else
        wakeup_source_trash(&fpc1020->ttw_wl);
#endif

        dev_info(fpc1020->dev, "%s: removed\n", __func__);
        return 0;
}

/* -------------------------------------------------------------------- */
static int fpc1020_spi_probe(struct spi_device *spi)
{
        //struct device *dev = &spi->dev;
        int error = 0;
        fpc1020_data_t *fpc1020 = NULL;
        /* size_t buffer_size; */

        fpc1020 = kzalloc(sizeof(*fpc1020), GFP_KERNEL);
        if (!fpc1020) {
                pr_err("failed to allocate memory for struct fpc1020_data\n");
                return -ENOMEM;
        }

        spi_set_drvdata(spi, fpc1020);
        fpc1020->spi = spi;

        fpc_ms=spi_master_get_devdata(spi->master);
        //pr_err("%s end\n",__func__);
        globle_spi_clk = fpc_ms->spi_clk;
        return error;
}

/* -------------------------------------------------------------------- */
static int fpc1020_spi_remove(struct spi_device *spi)
{
        return 0;
}


static struct of_device_id fpc1020_of_match[] = {
        { .compatible = "fpc,fpc_irq", },
        {}
};

MODULE_DEVICE_TABLE(of, fpc1020_of_match);

static struct of_device_id fpc1020_spi_of_match[] = {
        { .compatible = "fpc,fpc1020", },
        {}
};

MODULE_DEVICE_TABLE(of, fpc1020_spi_of_match);

static struct platform_driver fpc1020_irq_driver = {
        .driver = {
                .name                   = "fpc_irq",
                .owner                  = THIS_MODULE,
                .of_match_table = fpc1020_of_match,
         },
        .probe  = fpc1020_irq_probe,
        .remove = fpc1020_irq_remove
};

static struct spi_driver fpc1020_spi_driver = {
        .driver = {
                .name   = "fpc1020",
                .owner  = THIS_MODULE,
                .bus    = &spi_bus_type,
#ifdef CONFIG_OF
                .of_match_table = fpc1020_spi_of_match,
#endif
        },
        .probe  = fpc1020_spi_probe,
        .remove = fpc1020_spi_remove
};

#if 0
static struct spi_board_info fpc1020_spi_board_devs[] __initdata = {
        [0] = {
                .modalias="fpc1020",
                .bus_num = 0,// .bus_num = 0,  select spi0
                .chip_select=0,
                .mode = SPI_MODE_0,
                .max_speed_hz = 4000000,
        },
};
#endif

static int __init fpc1020_init(void)
{
        //int rc = 0;
        #if 0
        rc = spi_register_board_info(fpc1020_spi_board_devs, ARRAY_SIZE(fpc1020_spi_board_devs));
        if (rc)
        {
                pr_err("spi register board info");
                return -EINVAL;
        }
        #endif
        if (spi_register_driver(&fpc1020_spi_driver))
        {
                pr_err("register spi driver fail");
                return -EINVAL;
        }
        if(platform_driver_register(&fpc1020_irq_driver) ){
                pr_err("platform_driver_register fail");
                return -EINVAL;
        }
        return 0;
}

static void __exit fpc1020_exit(void)
{
	pr_err("[DEBUG]fpc1020_exit++++++++++++++++++++++\n");
        platform_driver_unregister(&fpc1020_irq_driver);
        spi_unregister_driver(&fpc1020_spi_driver);
        //platform_device_unregister(fpc_irq_platform_device);
}

#if CONFIG_OPPO_FINGERPRINT_PROJCT == 18151
late_initcall(fpc1020_init);
#else
module_init(fpc1020_init);
#endif

module_exit(fpc1020_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Aleksej Makarov");
MODULE_AUTHOR("Henrik Tillman <henrik.tillman@fingerprints.com>");
MODULE_DESCRIPTION("FPC1020 Fingerprint sensor device driver.");
