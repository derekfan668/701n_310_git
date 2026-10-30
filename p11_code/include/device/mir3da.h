
/******************** (C) COPYRIGHT 2018 MiraMEMS *****************************
* File Name     : mir3da.h
* Author        : ycwang@miramems.com
* Version       : V1.0
* Date          : 05/18/2018
* Description   : Demo for configuring mir3da
*******************************************************************************/
#ifndef __MIR3DA_h
#define __MIR3DA_h
#ifdef TCFG_MIR3DA_EN && TCFG_MIR3DA_EN

/*******************************************************************************
Macro definitions - Register define for Gsensor asic
********************************************************************************/
#define NSA_REG_SPI_I2C                 0x00
#define NSA_REG_WHO_AM_I                0x01
#define NSA_REG_ACC_X_LSB               0x02
#define NSA_REG_ACC_X_MSB               0x03
#define NSA_REG_ACC_Y_LSB               0x04
#define NSA_REG_ACC_Y_MSB               0x05
#define NSA_REG_ACC_Z_LSB               0x06
#define NSA_REG_ACC_Z_MSB               0x07
#define NSA_REG_MOTION_FLAG             0x09
#define NSA_REG_STEPS_MSB               0x0D
#define NSA_REG_STEPS_LSB               0x0E
#define NSA_REG_G_RANGE                 0x0F
#define NSA_REG_ODR_AXIS_DISABLE        0x10
#define NSA_REG_POWERMODE_BW            0x11
#define NSA_REG_SWAP_POLARITY           0x12
#define NSA_REG_FIFO_CTRL               0x14
#define NAS_REG_INT_SET0                0x15
#define NSA_REG_INTERRUPT_SETTINGS1     0x16
#define NSA_REG_INTERRUPT_SETTINGS2     0x17
#define NSA_REG_INTERRUPT_MAPPING1      0x19
#define NSA_REG_INTERRUPT_MAPPING2      0x1a
#define NSA_REG_INTERRUPT_MAPPING3      0x1b
#define NSA_REG_INT_PIN_CONFIG          0x20
#define NSA_REG_INT_LATCH               0x21
#define NSA_REG_ACTIVE_DURATION         0x27
#define NSA_REG_ACTIVE_THRESHOLD        0x28
#define NSA_REG_TAP_DURATION            0x2A
#define NSA_REG_TAP_THRESHOLD           0x2B
#define NSA_REG_STEP_CONFIG1            0x2F
#define NSA_REG_STEP_CONFIG2            0x30
#define NSA_REG_STEP_CONFIG3            0x31
#define NSA_REG_STEP_CONFIG4            0x32
#define NSA_REG_STEP_FILTER             0x33
#define NSA_REG_SM_THRESHOLD            0x34
#define NSA_REG_CUSTOM_OFFSET_X         0x38
#define NSA_REG_CUSTOM_OFFSET_Y         0x39
#define NSA_REG_CUSTOM_OFFSET_Z         0x3a
#define NSA_REG_ENGINEERING_MODE        0x7f
#define NSA_REG_SENSITIVITY_TRIM_X      0x80
#define NSA_REG_SENSITIVITY_TRIM_Y      0x81
#define NSA_REG_SENSITIVITY_TRIM_Z      0x82
#define NSA_REG_COARSE_OFFSET_TRIM_X    0x83
#define NSA_REG_COARSE_OFFSET_TRIM_Y    0x84
#define NSA_REG_COARSE_OFFSET_TRIM_Z    0x85
#define NSA_REG_FINE_OFFSET_TRIM_X      0x86
#define NSA_REG_FINE_OFFSET_TRIM_Y      0x87
#define NSA_REG_FINE_OFFSET_TRIM_Z      0x88
#define NSA_REG_SENS_COMP               0x8c
#define NSA_REG_MEMS_OPTION             0x8f
#define NSA_REG_CHIP_INFO               0xc0
#define NSA_REG_CHIP_INFO_SECOND        0xc1
#define NSA_REG_MEMS_OPTION_SECOND      0xc7
#define NSA_REG_SENS_COARSE_TRIM        0xd1
#define NAS_REG_OSC_TRIM                0x8e
#endif
#endif
