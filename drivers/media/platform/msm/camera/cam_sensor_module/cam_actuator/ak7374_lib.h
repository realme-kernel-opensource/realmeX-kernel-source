#ifndef _AK7374_LIB_
#define _AK7374_LIB_

#include <cam_sensor_cmn_header.h>

struct  cam_sensor_i2c_reg_array  ak7374_imx362_write_reg_setting[] =
{
    { 0xAE, 0x3B, 0x0A, 0x00},
};

struct cam_sensor_i2c_reg_setting ak7374_imx362_write_reg = {
	.reg_setting = ak7374_imx362_write_reg_setting,
	.size = 1,
	.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.delay = 0x00,
};

struct  cam_sensor_i2c_reg_array  ak7374_imx362_reg_setting[] =
{
    { 0x10, 0x55, 0x00, 0x00},
    { 0x11, 0x28, 0x00, 0x00},
    { 0x12, 0x78, 0x00, 0x00},
    { 0x13, 0x22, 0x00, 0x00},
    { 0x14, 0x20, 0x00, 0x00},
    { 0x15, 0x78, 0x00, 0x00},
    { 0x16, 0x11, 0x00, 0x00},
    { 0x17, 0x50, 0x00, 0x00},
    { 0x18, 0xDB, 0x00, 0x00},
    { 0x19, 0x00, 0x00, 0x00},
    { 0x1A, 0x23, 0x00, 0x00},
    { 0x1C, 0x00, 0x00, 0x00},
    { 0x1D, 0x00, 0x00, 0x00},
    { 0x1E, 0x00, 0x00, 0x00},
    { 0x20, 0x1E, 0x00, 0x00},
    { 0x21, 0x19, 0x00, 0x00},
    { 0x22, 0x14, 0x00, 0x00},
    { 0x23, 0x0B, 0x00, 0x00},
    { 0x24, 0x00, 0x00, 0x00},
    { 0x25, 0x00, 0x00, 0x00},
};

struct cam_sensor_i2c_reg_setting ak7374_imx362_reg = {
	.reg_setting = ak7374_imx362_reg_setting,
	.size = 20,
	.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.delay = 0x00,
};

struct  cam_sensor_i2c_reg_array  ak7374_imx362_check_status_setting[] =
{
    { 0x4B, (0x1<<2), 0x5, 0x00},
};

struct cam_sensor_i2c_reg_setting ak7374_imx362_check_status = {
	.reg_setting = ak7374_imx362_check_status_setting,
	.size = 1,
	.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.delay = 0x00,
};

struct  cam_sensor_i2c_reg_array  ak7374_imx362_check_reg_setting[] =
{
    { 0x03, 0x10, 0x10, 0x00},
};

struct cam_sensor_i2c_reg_setting ak7374_imx362_check_reg = {
	.reg_setting = ak7374_imx362_check_reg_setting,
	.size = 1,
	.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
	.delay = 0x00,
};
#endif