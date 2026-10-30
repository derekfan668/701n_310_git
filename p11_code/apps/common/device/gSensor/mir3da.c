#include "app_config.h"
#include "gSensor_manage.h"
#include "includes.h"
#include "iic_api.h"
#include "bank_switch.h"
#include "gpio.h"
#include "mir3da.h"

/* #pragma bss_seg(".gsport_dev_bss") */
/* #pragma const_seg(".gsport_dev_const") */
/* #pragma code_seg(".gsport_dev_code") */
/*  */


#define BANK_NAME  BANK_SENSOR_TAG


#ifdef TCFG_MIR3DA_EN && TCFG_MIR3DA_EN

#define LOG(fmt,...)     printf("[mir3da] " fmt "\n",##__VA_ARGS__)


/***使用驱动前请根据实际接线情况配置（7bit）IIC地址******/
/**DA263的SAO 脚接浮空：           0x27****************/
/**DA263的SAO 脚接地：             0x26****************/
static  u8 i2c_addr = 0x27;

enum {
    MIR3_DA263,
    MIR3_DA267,
} sensor_model;


static s8 mir3da_register_read(u8 addr, u8 *data, u8 len)
{
    return !_gravity_sensor_get_ndata((i2c_addr << 1) | 0x01, addr, data, len);
}


static s8 mir3da_register_write(u8 addr, u8 data)
{
    return !gravity_sensor_command((i2c_addr << 1) | 0x00, addr, data);
}


static s8 mir3da_is_online(void)
{
    u8 data = 0;

    for (u8 i = 0; i < 3; i++) {
        mir3da_register_read(NSA_REG_WHO_AM_I, &data, 1);
        if (data == 0x13) {
            break;
        }
    }

    LOG("device id=0x%02x", data);
    return data == 0x13;
}

static void mir3da_model_check(void)
{
    u8 data = 0;
    mir3da_register_read(0x11, &data, 1);
    if (data == 0x80) {
        sensor_model = MIR3_DA267; //reg11 Default data: 0x80
    } else {
        sensor_model = MIR3_DA263; //reg11 Default data: 0x9E
    }
    LOG("reg11=0x%02x model=%s", data, sensor_model == MIR3_DA267 ? "da267" : "da263");
}

static s8 mir3da_register_mask_write(unsigned char addr, unsigned char mask, unsigned char data)
{
    int           res = 0;
    unsigned char tmp_data;

    res = mir3da_register_read(addr, &tmp_data, 1);
    if (res) {
        return res;
    }

    tmp_data &= ~mask;
    tmp_data |= data & mask;
    res = mir3da_register_write(addr, tmp_data);

    return res;
}


//Initialization
u8 mir3da_init(void)
{
    s8 res = 0;
    u8 data = 0;

    if (!mir3da_is_online()) {
        LOG("fail!");
        return -1;
    }

    mir3da_register_mask_write(0x00, 0x24, 0x24);  //soft reset
    mdelay(50);
    mir3da_model_check();
    mir3da_register_write(0x0F, 0x01);  //+/-4G,14bit(MIR3_DA263) or 16bit(MIR3_DA267)

    if (sensor_model == MIR3_DA263) {
        mir3da_register_write(0x10, 0x05);  //ODR =31.25hz
        mir3da_register_write(0x11, 0x14);  //normal mode
        //打开工程模式
        mir3da_register_write(NSA_REG_ENGINEERING_MODE, 0x83);
        mir3da_register_write(NSA_REG_ENGINEERING_MODE, 0x69);
        mir3da_register_write(NSA_REG_ENGINEERING_MODE, 0xBD);
        mir3da_register_mask_write(NAS_REG_OSC_TRIM, 0xf0, 0x00);  //ODR=25hz
    } else if (sensor_model == MIR3_DA267) {
        mir3da_register_write(0x10, 0x15);  //ODR = 25hz
        mir3da_register_write(0x11, 0x00);  //normal mode
    }


    //Reduce power consumption
    if (i2c_addr == 0x26) {
        //关闭1脚上拉
        mir3da_register_mask_write(NSA_REG_SENS_COMP, 0x40, 0x00);
    }

    mir3da_register_write(0x14, 0x80); //stream mode
    return 0;
}

s8 mir3da_set_enable(u8 enable)
{
    s8 res   = 0;
    u8 reg11 = sensor_model == MIR3_DA263 ? 0x14 : 0x00;

    if (enable) {
        res = mir3da_register_write(NSA_REG_POWERMODE_BW, reg11);
    } else {
        res = mir3da_register_write(NSA_REG_POWERMODE_BW, 0x80);
    }

    return !res;
}


//Read three axis data, 1024 LSB = 1 g
s8 mir3da_read_data(s16 *x, s16 *y, s16 *z)
{
    u8 tmp_data[6] = {0};

    if (mir3da_register_read(NSA_REG_ACC_X_LSB, tmp_data, 6) != 0) {
        return -1;
    }

    *x = ((s16)(tmp_data[1] << 8 | tmp_data[0])) >> 3;
    *y = ((s16)(tmp_data[3] << 8 | tmp_data[2])) >> 3;
    *z = ((s16)(tmp_data[5] << 8 | tmp_data[4])) >> 3;

    if (*x == 0 && *y == 0 && *z == 0) {
        return -1;
    }
    LOG("xyz:%d,%d,%d", *x, *y, *z);
    return 0;
}

int mir3da_open_full_fifo_int()
{
    int   res = 0;
    res = mir3da_register_write(0x14, 0x80);
    res = mir3da_register_write(0x15, 0x04);
    res = mir3da_register_write(0x1A, 0x04);
    return res;
}


//num 1-31
int mir3da_open_fifo_watermark_int(u8 num)
{
    int   res = 0;
    res = mir3da_register_write(0x14, 0x80 | num);
    res = mir3da_register_write(0x15, 0x08);
    res = mir3da_register_write(0x1A, 0x02);
    return res;
}

int mir3da_close_fifo_int(void)
{
    int   res = 0;
    res = mir3da_register_write(0x14, 0x00);
    res = mir3da_register_write(0x15, 0x00);
    res = mir3da_register_write(0x1A, 0x00);
    return res;
}


int mir3da_read_full_fifo(short *x, short *y, short *z)
{
    u8 i = 0, tmp_data = 0;

    mir3da_register_read(0x08, &tmp_data, 1);

    if ((tmp_data & 0x40)) {
        for (i = 0; i < 32; i++) {
            mir3da_read_data(x + i, y + i, z + i);
        }
    }

    return 0;
}

int mir3da_read_fifo(axis_info_t *accel)
{
    u8 fifo_len = 0;

    mir3da_register_read(0x08, &fifo_len, 1);
    fifo_len &= 0x3f;

    LOG("fifo_len=%d", fifo_len);

    for (u8 i = 0; i < fifo_len; i++) {
        if (mir3da_read_data(&accel[i].x, &accel[i].y, &accel[i].z) == -1) {
            fifo_len = i;
        }
    }

    return fifo_len;
}


s32 mir3da_ctl(u8 cmd, void *arg)
{
    char res;
    s32 ret = 0;
    LOG("cmd=%d", cmd);
    switch (cmd) {
    case GSENSOR_DISABLE:
        res = mir3da_set_enable(0);
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESET_INT:
        res = mir3da_init();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESUME_INT:
        break;
    case GSENSOR_INT_DET:
        break;
    case READ_GSENSOR_DATA:
        ret = mir3da_read_fifo((axis_info_t *)arg);
        break;
    case SEARCH_SENSOR:
        res = mir3da_is_online();
        memcpy(arg, &res, 1);
        break;
    default:
        break;
    }
    return ret;
}


REGISTER_GRAVITY_SENSOR(gSensor) = {
    .logo = "mir3da",
    .gravity_sensor_init  = mir3da_init,
    .gravity_sensor_check = NULL,
    .gravity_sensor_ctl   = mir3da_ctl,
};

#endif
