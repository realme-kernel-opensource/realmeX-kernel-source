/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "cam_actuator_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_actuator_soc.h"
#include "cam_actuator_core.h"
#include "cam_trace.h"
#ifdef VENDOR_EDIT
/*Added by Jinshui.Liu@Camera 20161125 for [debug interface]*/
#include <linux/proc_fs.h>
#endif

#ifdef VENDOR_EDIT
/*Jinshui.Liu@Camera.Driver, 2018/06/22, add for [Iris debug]*/
struct cam_actuator_ctrl_t *g_a_ctrl = NULL;
int Iris_value = 0;
#endif

static long cam_actuator_subdev_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, void *arg)
{
	int rc = 0;
	struct cam_actuator_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		rc = cam_actuator_driver_cmd(a_ctrl, arg);
		break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid ioctl cmd");
		rc = -EINVAL;
		break;
	}
	return rc;
}

#ifdef CONFIG_COMPAT
static long cam_actuator_init_subdev_do_ioctl(struct v4l2_subdev *sd,
	unsigned int cmd, unsigned long arg)
{
	struct cam_control cmd_data;
	int32_t rc = 0;

	if (copy_from_user(&cmd_data, (void __user *)arg,
		sizeof(cmd_data))) {
		CAM_ERR(CAM_ACTUATOR,
			"Failed to copy from user_ptr=%pK size=%zu",
			(void __user *)arg, sizeof(cmd_data));
		return -EFAULT;
	}

	switch (cmd) {
	case VIDIOC_CAM_CONTROL:
		cmd = VIDIOC_CAM_CONTROL;
		rc = cam_actuator_subdev_ioctl(sd, cmd, &cmd_data);
		if (rc) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed in actuator subdev handling rc: %d",
				rc);
			return rc;
		}
	break;
	default:
		CAM_ERR(CAM_ACTUATOR, "Invalid compat ioctl: %d", cmd);
		rc = -EINVAL;
	}

	if (!rc) {
		if (copy_to_user((void __user *)arg, &cmd_data,
			sizeof(cmd_data))) {
			CAM_ERR(CAM_ACTUATOR,
				"Failed to copy to user_ptr=%pK size=%zu",
				(void __user *)arg, sizeof(cmd_data));
			rc = -EFAULT;
		}
	}
	return rc;
}
#endif

static int cam_actuator_subdev_close(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh)
{
	struct cam_actuator_ctrl_t *a_ctrl =
		v4l2_get_subdevdata(sd);

	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "a_ctrl ptr is NULL");
		return -EINVAL;
	}

	mutex_lock(&(a_ctrl->actuator_mutex));
	cam_actuator_shutdown(a_ctrl);
	mutex_unlock(&(a_ctrl->actuator_mutex));

	return 0;
}

static struct v4l2_subdev_core_ops cam_actuator_subdev_core_ops = {
	.ioctl = cam_actuator_subdev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = cam_actuator_init_subdev_do_ioctl,
#endif
};

static struct v4l2_subdev_ops cam_actuator_subdev_ops = {
	.core = &cam_actuator_subdev_core_ops,
};

static const struct v4l2_subdev_internal_ops cam_actuator_internal_ops = {
	.close = cam_actuator_subdev_close,
};

static int cam_actuator_init_subdev(struct cam_actuator_ctrl_t *a_ctrl)
{
	int rc = 0;

	a_ctrl->v4l2_dev_str.internal_ops =
		&cam_actuator_internal_ops;
	a_ctrl->v4l2_dev_str.ops =
		&cam_actuator_subdev_ops;
	strlcpy(a_ctrl->device_name, CAMX_ACTUATOR_DEV_NAME,
		sizeof(a_ctrl->device_name));
	a_ctrl->v4l2_dev_str.name =
		a_ctrl->device_name;
	a_ctrl->v4l2_dev_str.sd_flags =
		(V4L2_SUBDEV_FL_HAS_DEVNODE | V4L2_SUBDEV_FL_HAS_EVENTS);
	a_ctrl->v4l2_dev_str.ent_function =
		CAM_ACTUATOR_DEVICE_TYPE;
	a_ctrl->v4l2_dev_str.token = a_ctrl;

	rc = cam_register_subdev(&(a_ctrl->v4l2_dev_str));
	if (rc)
		CAM_ERR(CAM_SENSOR, "Fail with cam_register_subdev rc: %d", rc);

	return rc;
}

#ifdef VENDOR_EDIT
/*Modified by Yingpiao.Lin@Cam.Drv, 20180717, for iris flow*/
static ssize_t actuator_proc_write(struct file *filp, const char __user *buff,
	size_t len, loff_t *data)
{
	char buf[32] = {0};
	uint16_t read_write_flag = 0;
	uint16_t store_sid = 0;
	unsigned long buf_value;
	int rc = 0;
	struct cam_sensor_i2c_reg_setting write_setting;
	struct cam_sensor_i2c_reg_array reg_setting[10];

	if (!g_a_ctrl || g_a_ctrl->cam_act_state == CAM_ACTUATOR_INIT) {
		if (g_a_ctrl)
			CAM_ERR(CAM_ACTUATOR, "actuator_state %d", g_a_ctrl->cam_act_state);
		return len;
	}

	if (len > 32)
		len = 32;
	if (copy_from_user(buf, buff, len)) {
		CAM_ERR(CAM_ACTUATOR, "proc write copy_from_user error ");
		return -EFAULT;
	}
	buf_value = simple_strtoul(buf, NULL, 16);
	read_write_flag = buf_value;

	if (Iris_value == read_write_flag) {
		return len;
	} else {
		Iris_value = read_write_flag;
	}

	/*1 init, 2 small, 3 large*/
	if (read_write_flag == 1) {
		reg_setting[0].reg_addr = 0xae;
		reg_setting[0].reg_data = 0x3b;
		reg_setting[0].delay = 0;
		reg_setting[0].data_mask = 0;
		reg_setting[1].reg_addr = 0x02;
		reg_setting[1].reg_data = 0x00;
		reg_setting[1].delay = 0;
		reg_setting[1].data_mask = 0;
		reg_setting[2].reg_addr = 0xa6;
		reg_setting[2].reg_data = 0x7b;
		reg_setting[2].delay = 10;
		reg_setting[2].data_mask = 0;
		reg_setting[3].reg_addr = 0xa6;
		reg_setting[3].reg_data = 0x00;
		reg_setting[3].delay = 15;
		reg_setting[3].data_mask = 0;
	} else if (read_write_flag == 2) {
		reg_setting[0].reg_addr = 0xa6;
		reg_setting[0].reg_data = 0x7b;
		reg_setting[0].delay = 10;
		reg_setting[0].data_mask = 0;
		reg_setting[1].reg_addr = 0x00;
		reg_setting[1].reg_data = 0xff;
		reg_setting[1].delay = 0;
		reg_setting[1].data_mask = 0;
		reg_setting[2].reg_addr = 0x01;
		reg_setting[2].reg_data = 0xc0;
		reg_setting[2].delay = 0;
		reg_setting[2].data_mask = 0;
		reg_setting[3].reg_addr = 0xa6;
		reg_setting[3].reg_data = 0x00;
		reg_setting[3].delay = 15;
		reg_setting[3].data_mask = 0;
	} else if (read_write_flag == 3) {
		reg_setting[0].reg_addr = 0xa6;
		reg_setting[0].reg_data = 0x7b;
		reg_setting[0].delay = 10;
		reg_setting[0].data_mask = 0;
		reg_setting[1].reg_addr = 0x00;
		reg_setting[1].reg_data = 0x00;
		reg_setting[1].delay = 0;
		reg_setting[1].data_mask = 0;
		reg_setting[2].reg_addr = 0x01;
		reg_setting[2].reg_data = 0x00;
		reg_setting[2].delay = 0;
		reg_setting[2].data_mask = 0;
		reg_setting[3].reg_addr = 0xa6;
		reg_setting[3].reg_data = 0x00;
		reg_setting[3].delay = 15;
		reg_setting[3].data_mask = 0;
	} else {
		CAM_ERR(CAM_ACTUATOR, "unsupported type!!");
		return len;
	}

	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 1;
	write_setting.size = 1;
	write_setting.reg_setting = &reg_setting[0];
	store_sid = g_a_ctrl->io_master_info.cci_client->sid;
	g_a_ctrl->io_master_info.cci_client->sid = 0x98 >> 1;
	rc = camera_io_dev_write(&g_a_ctrl->io_master_info,
		&write_setting);
	g_a_ctrl->io_master_info.cci_client->sid = store_sid;;
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "read/write err");
	}

	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 1;
	write_setting.size = 1;
	write_setting.reg_setting = &reg_setting[1];
	store_sid = g_a_ctrl->io_master_info.cci_client->sid;
	g_a_ctrl->io_master_info.cci_client->sid = 0x98 >> 1;
	rc = camera_io_dev_write(&g_a_ctrl->io_master_info,
		&write_setting);
	g_a_ctrl->io_master_info.cci_client->sid = store_sid;;
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "read/write err");
	}

	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 10;
	write_setting.size = 1;
	write_setting.reg_setting = &reg_setting[2];
	store_sid = g_a_ctrl->io_master_info.cci_client->sid;
	g_a_ctrl->io_master_info.cci_client->sid = 0x98 >> 1;
	rc = camera_io_dev_write(&g_a_ctrl->io_master_info,
		&write_setting);
	g_a_ctrl->io_master_info.cci_client->sid = store_sid;;
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "read/write err");
	}

	write_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	write_setting.delay = 15;
	write_setting.size = 1;
	write_setting.reg_setting = &reg_setting[3];
	store_sid = g_a_ctrl->io_master_info.cci_client->sid;
	g_a_ctrl->io_master_info.cci_client->sid = 0x98 >> 1;
	rc = camera_io_dev_write(&g_a_ctrl->io_master_info,
		&write_setting);
	g_a_ctrl->io_master_info.cci_client->sid = store_sid;;
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "read/write err");
	}

	return len;
}

static ssize_t actuator_proc_read(struct file *filp, char __user *buff,
	size_t len, loff_t *data)
{
	if (!g_a_ctrl || g_a_ctrl->cam_act_state == CAM_ACTUATOR_INIT) {
		if (g_a_ctrl)
			CAM_ERR(CAM_ACTUATOR, "actuator_state %d", g_a_ctrl->cam_act_state);
		return 0;
	}

	CAM_ERR(CAM_ACTUATOR, "slave 0x%02x",
		g_a_ctrl->io_master_info.cci_client->sid);
	return 0;
}

static const struct file_operations actuator_fops = {
	.owner = THIS_MODULE,
	.read = actuator_proc_read,
	.write = actuator_proc_write,
};

static int cam_actuator_proc_init(void)
{
	int ret = 0;

	struct proc_dir_entry *proc_entry;

	proc_entry = proc_create_data( "Iris_debug", 0666, NULL, &actuator_fops, NULL);
	if (proc_entry == NULL) {
		ret = -ENOMEM;
		CAM_ERR(CAM_ACTUATOR, "Error! Couldn't create qcom_actuator proc entry");
	}

	return ret;
}
#endif

static int32_t cam_actuator_driver_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t                         rc = 0;
	int32_t                         i = 0;
	struct cam_actuator_ctrl_t      *a_ctrl;
	struct cam_hw_soc_info          *soc_info = NULL;
	struct cam_actuator_soc_private *soc_private = NULL;

	if (client == NULL || id == NULL) {
		CAM_ERR(CAM_ACTUATOR, "Invalid Args client: %pK id: %pK",
			client, id);
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CAM_ERR(CAM_ACTUATOR, "%s :: i2c_check_functionality failed",
			 client->name);
		rc = -EFAULT;
		return rc;
	}

	/* Create sensor control structure */
	a_ctrl = kzalloc(sizeof(*a_ctrl), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	i2c_set_clientdata(client, a_ctrl);

	soc_private = kzalloc(sizeof(struct cam_actuator_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_ctrl;
	}
	a_ctrl->soc_info.soc_private = soc_private;

	a_ctrl->io_master_info.client = client;
	soc_info = &a_ctrl->soc_info;
	soc_info->dev = &client->dev;
	soc_info->dev_name = client->name;
	a_ctrl->io_master_info.master_type = I2C_MASTER;

	rc = cam_actuator_parse_dt(a_ctrl, &client->dev);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "failed: cam_sensor_parse_dt rc %d", rc);
		goto free_soc;
	}

	rc = cam_actuator_init_subdev(a_ctrl);
	if (rc)
		goto free_soc;

	if (soc_private->i2c_info.slave_addr != 0)
		a_ctrl->io_master_info.client->addr =
			soc_private->i2c_info.slave_addr;

	a_ctrl->i2c_data.per_frame =
		(struct i2c_settings_array *)
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto unreg_subdev;
	}

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_actuator_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_actuator_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_actuator_apply_request;

	v4l2_set_subdevdata(&(a_ctrl->v4l2_dev_str.sd), a_ctrl);

	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;

	return rc;

unreg_subdev:
	cam_unregister_subdev(&(a_ctrl->v4l2_dev_str));
free_soc:
	kfree(soc_private);
free_ctrl:
	kfree(a_ctrl);
	return rc;
}

static int32_t cam_actuator_platform_remove(struct platform_device *pdev)
{
	int32_t rc = 0;
	struct cam_actuator_ctrl_t      *a_ctrl;
	struct cam_actuator_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t  *power_info;

	a_ctrl = platform_get_drvdata(pdev);
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Actuator device is NULL");
		return 0;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	kfree(a_ctrl->io_master_info.cci_client);
	a_ctrl->io_master_info.cci_client = NULL;
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	kfree(a_ctrl->soc_info.soc_private);
	kfree(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	#ifdef VENDOR_EDIT
	/*Modified by Yingpiao.Lin@Cam.Drv, 20180717, for iris flow*/
	if (a_ctrl->piris_ctrl) {
		devm_kfree(&pdev->dev, a_ctrl->piris_ctrl);
	}
	#endif
	devm_kfree(&pdev->dev, a_ctrl);

	return rc;
}

static int32_t cam_actuator_driver_i2c_remove(struct i2c_client *client)
{
	int32_t rc = 0;
	struct cam_actuator_ctrl_t      *a_ctrl =
		i2c_get_clientdata(client);
	struct cam_actuator_soc_private *soc_private;
	struct cam_sensor_power_ctrl_t  *power_info;

	/* Handle I2C Devices */
	if (!a_ctrl) {
		CAM_ERR(CAM_ACTUATOR, "Actuator device is NULL");
		return -EINVAL;
	}

	soc_private =
		(struct cam_actuator_soc_private *)a_ctrl->soc_info.soc_private;
	power_info = &soc_private->power_info;

	/*Free Allocated Mem */
	kfree(a_ctrl->i2c_data.per_frame);
	a_ctrl->i2c_data.per_frame = NULL;
	kfree(power_info->power_setting);
	kfree(power_info->power_down_setting);
	kfree(a_ctrl->soc_info.soc_private);
	a_ctrl->soc_info.soc_private = NULL;
	kfree(a_ctrl);
	return rc;
}

static const struct of_device_id cam_actuator_driver_dt_match[] = {
	{.compatible = "qcom,actuator"},
	{}
};

static int32_t cam_actuator_driver_platform_probe(
	struct platform_device *pdev)
{
	int32_t                         rc = 0;
	int32_t                         i = 0;
	struct cam_actuator_ctrl_t      *a_ctrl = NULL;
	struct cam_actuator_soc_private *soc_private = NULL;

	/* Create actuator control structure */
	a_ctrl = devm_kzalloc(&pdev->dev,
		sizeof(struct cam_actuator_ctrl_t), GFP_KERNEL);
	if (!a_ctrl)
		return -ENOMEM;

	/*fill in platform device*/
	a_ctrl->v4l2_dev_str.pdev = pdev;
	a_ctrl->soc_info.pdev = pdev;
	a_ctrl->soc_info.dev = &pdev->dev;
	a_ctrl->soc_info.dev_name = pdev->name;
	a_ctrl->io_master_info.master_type = CCI_MASTER;

	a_ctrl->io_master_info.cci_client = kzalloc(sizeof(
		struct cam_sensor_cci_client), GFP_KERNEL);
	if (!(a_ctrl->io_master_info.cci_client)) {
		rc = -ENOMEM;
		goto free_ctrl;
	}

	soc_private = kzalloc(sizeof(struct cam_actuator_soc_private),
		GFP_KERNEL);
	if (!soc_private) {
		rc = -ENOMEM;
		goto free_cci_client;
	}
	a_ctrl->soc_info.soc_private = soc_private;
	soc_private->power_info.dev = &pdev->dev;

	a_ctrl->i2c_data.per_frame =
		(struct i2c_settings_array *)
		kzalloc(sizeof(struct i2c_settings_array) *
		MAX_PER_FRAME_ARRAY, GFP_KERNEL);
	if (a_ctrl->i2c_data.per_frame == NULL) {
		rc = -ENOMEM;
		goto free_soc;
	}

	INIT_LIST_HEAD(&(a_ctrl->i2c_data.init_settings.list_head));

	for (i = 0; i < MAX_PER_FRAME_ARRAY; i++)
		INIT_LIST_HEAD(&(a_ctrl->i2c_data.per_frame[i].list_head));

	rc = cam_actuator_parse_dt(a_ctrl, &(pdev->dev));
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR, "Paring actuator dt failed rc %d", rc);
		goto free_mem;
	}

	/* Fill platform device id*/
	pdev->id = a_ctrl->soc_info.index;

	rc = cam_actuator_init_subdev(a_ctrl);
	if (rc)
		goto free_mem;

	a_ctrl->bridge_intf.device_hdl = -1;
	a_ctrl->bridge_intf.ops.get_dev_info =
		cam_actuator_publish_dev_info;
	a_ctrl->bridge_intf.ops.link_setup =
		cam_actuator_establish_link;
	a_ctrl->bridge_intf.ops.apply_req =
		cam_actuator_apply_request;
	a_ctrl->bridge_intf.ops.flush_req =
		cam_actuator_flush_request;

	platform_set_drvdata(pdev, a_ctrl);
	v4l2_set_subdevdata(&a_ctrl->v4l2_dev_str.sd, a_ctrl);
	a_ctrl->cam_act_state = CAM_ACTUATOR_INIT;

#ifdef VENDOR_EDIT
	/*Modified by Yingpiao.Lin@Cam.Drv, 20180717, for iris flow*/
	if (a_ctrl->soc_info.index == 0) {
		a_ctrl->piris_ctrl = devm_kzalloc(&pdev->dev,
			sizeof(struct cam_actuator_ctrl_t), GFP_KERNEL);
		if (a_ctrl->piris_ctrl) {
			memcpy(a_ctrl->piris_ctrl, a_ctrl, sizeof(struct cam_actuator_ctrl_t));
		} else {
			CAM_ERR(CAM_ACTUATOR, "a_ctrl->piris_ctrl is NULL");
		}
		g_a_ctrl = a_ctrl;
		cam_actuator_proc_init();
	}

	/*Added by Zhengrong.Zhang@Cam.Drv, 2018/11/08, for update ak7374 PID*/
	a_ctrl->is_check_firmware_update = 1;
#endif

	return rc;

free_mem:
	kfree(a_ctrl->i2c_data.per_frame);
free_soc:
	kfree(soc_private);
free_cci_client:
	kfree(a_ctrl->io_master_info.cci_client);
free_ctrl:
#ifdef VENDOR_EDIT
	/*Modified by Yingpiao.Lin@Cam.Drv, 20180717, for iris flow*/
	if (a_ctrl->piris_ctrl) {
		devm_kfree(&pdev->dev, a_ctrl->piris_ctrl);
	}
#endif
	devm_kfree(&pdev->dev, a_ctrl);
	return rc;
}

MODULE_DEVICE_TABLE(of, cam_actuator_driver_dt_match);

static struct platform_driver cam_actuator_platform_driver = {
	.probe = cam_actuator_driver_platform_probe,
	.driver = {
		.name = "qcom,actuator",
		.owner = THIS_MODULE,
		.of_match_table = cam_actuator_driver_dt_match,
	},
	.remove = cam_actuator_platform_remove,
};

static const struct i2c_device_id i2c_id[] = {
	{ACTUATOR_DRIVER_I2C, (kernel_ulong_t)NULL},
	{ }
};

static struct i2c_driver cam_actuator_driver_i2c = {
	.id_table = i2c_id,
	.probe  = cam_actuator_driver_i2c_probe,
	.remove = cam_actuator_driver_i2c_remove,
	.driver = {
		.name = ACTUATOR_DRIVER_I2C,
	},
};

static int __init cam_actuator_driver_init(void)
{
	int32_t rc = 0;

	rc = platform_driver_register(&cam_actuator_platform_driver);
	if (rc < 0) {
		CAM_ERR(CAM_ACTUATOR,
			"platform_driver_register failed rc = %d", rc);
		return rc;
	}
	rc = i2c_add_driver(&cam_actuator_driver_i2c);
	if (rc)
		CAM_ERR(CAM_ACTUATOR, "i2c_add_driver failed rc = %d", rc);

	return rc;
}

static void __exit cam_actuator_driver_exit(void)
{
	platform_driver_unregister(&cam_actuator_platform_driver);
	i2c_del_driver(&cam_actuator_driver_i2c);
}

module_init(cam_actuator_driver_init);
module_exit(cam_actuator_driver_exit);
MODULE_DESCRIPTION("cam_actuator_driver");
MODULE_LICENSE("GPL v2");
