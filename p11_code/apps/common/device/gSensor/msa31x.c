#include "app_config.h"
#include "gSensor_manage.h"
#include "msa31x.h"

#if TCFG_MSA31X_EN

#define ADDR_R   ((0x62<<1) | 1)
#define ADDR_W   ((0x62<<1) | 0)

u8 msa_register_read(u8 addr, u8 *data)
{
    return !_gravity_sensor_get_ndata(ADDR_R, addr, data, 1);
}

u8  msa_register_write(u8 addr, u8 data)
{
    return !gravity_sensor_command(ADDR_W, addr, data);
}

u8  msa_register_read_continuously(u8 addr, u8 count, u8 *data)
{
    return _gravity_sensor_get_ndata(ADDR_R, addr, data, count);
}

u8  msa_register_mask_write(u8 addr, u8 mask, u8 data)
{
    u8  res = 0;
    u8  tmp_data;

    res = msa_register_read(addr, &tmp_data);
    if (res) {
        return res;
    }

    tmp_data &= ~mask;
    tmp_data |= data & mask;
    res = msa_register_write(addr, tmp_data);

    return res;
}


u8  msa_read_data(s16 *x, s16 *y, s16 *z)
{
    u8    tmp_data[6] = {0};

    if (msa_register_read_continuously(MSA_REG_ACC_X_LSB, 6, tmp_data) < 6) {
        return -1;
    }

    *x = ((short)(tmp_data[1] << 8 | tmp_data[0])) >> 4;
    *y = ((short)(tmp_data[3] << 8 | tmp_data[2])) >> 4;
    *z = ((short)(tmp_data[5] << 8 | tmp_data[4])) >> 4;

    dev_log(" %d %d %d", *x, *y, *z);

    return 0;
}


u8 msa31x_check(void)
{
    u8 ret  = 0;
    u8 data = 0;

    msa_register_read(MSA_REG_WHO_AM_I, &data);
    ret  = (data == 0x13);
    dev_log("id=0x%02x %s !", data, ret ? "ok" : "fail");
    return ret;
}

u8 msa31x_disable(void)
{
    u8  res = 0;

    res =  msa_register_write(MSA_REG_SPI_I2C,       0x24);  //soft reset
    mdelay(5);
    res |= msa_register_write(MSA_REG_POWERMODE_BW,  0xDE);  //suspend mode

    return !res;
}


u8 msa31x_init(void)
{
    u8  res  = 0;

    if (msa31x_check() == 0) {
        return -1;
    }

    res =  msa_register_write(0x00, 0x24);        //soft reset
    mdelay(5);

    res |= msa_register_write(0x0f,  0x01);       //FS[1:0]:  00:+/-2g, 01:+/-4g, 10:+/-8g, 11:+/-16g
    res |= msa_register_write(0x10,  0x04);       //ODR[3:0]:  0100: 15.63Hz 0101: 31.25Hz  0110: 62.5Hz  0111: 125Hz
    res |= msa_register_write(0x11,  0x7e);       //0x0a: normal mode  0x7e: low power mode
    res |= msa_register_write(0x14,  0x80 | 25);  //fifo: stream mode watermark 25
    res |= msa_register_write(0x15,  0x00);       //FIFO_DATA_SELECT[1:0]: 00: X+Y+Z 01: X only 10: Y only 11: Z only
    res |= msa_register_write(0x17,  0x60);       //enable overrun interrupt ?watermark interrupt
    res |= msa_register_write(0x1a,  0x06);       //overrun interrupt ?watermark interrupt to INT1

    const u8 reg_addr[] = {0x0f, 0x10, 0x11, 0x14, 0x15, 0x17, 0x1a};
    u8 data = 0;
    for (u8 i = 0; i < sizeof(reg_addr); i++) {
        msa_register_read(reg_addr[i], &data);
        dev_log("reg[0x%02x] = 0x%02x", reg_addr[i], data);
    }

    return 0;
}

void msa_set_fifo_mode(void)
{
    msa_register_write(0x14,  0x00 | 48);  //fifo: stream mode watermark 25
    mdelay(5);
    msa_register_write(0x14,  0x80 | 48);  //fifo: stream mode watermark 25
}

s32  msa31x_read_fifo(axis_info_t *sl_accel)
{
    u8 len = 0;
    u8 temp_buf[384];

    msa_register_read(0x0d, &temp_buf[0]); //read fifo status
    len = temp_buf[0] & 0x7f;

    dev_log("reg[0x%02x] = 0x%02x len=%d", 0x0d, temp_buf[0], len);

    msa_register_read_continuously(0xff, len * 6, temp_buf);

    for (u8 i = 0; i < len; i++) {
        sl_accel[i].x = ((short)(temp_buf[i * 6 + 1] << 8 | temp_buf[i * 6 + 0])) >> 3; //lsb_g = 1024
        sl_accel[i].y = ((short)(temp_buf[i * 6 + 3] << 8 | temp_buf[i * 6 + 2])) >> 3; //lsb_g = 1024
        sl_accel[i].z = ((short)(temp_buf[i * 6 + 5] << 8 | temp_buf[i * 6 + 4])) >> 3; //lsb_g = 1024

        dev_log(" %d %d %d", sl_accel[i].x, sl_accel[i].y, sl_accel[i].z);
    }

    return len;
}

s32 msa31x_ctl(u8 cmd, void *arg)
{
    char res;
    s32 ret = 0;

    switch (cmd) {
    case GSENSOR_DISABLE:
        res = msa31x_disable();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESET_INT:
        res = msa31x_init();
        memcpy(arg, &res, 1);
        break;
    case GSENSOR_RESUME_INT:
        break;
    case GSENSOR_INT_DET:
        break;
    case READ_GSENSOR_DATA:
        ret = msa31x_read_fifo((axis_info_t *)arg);
        break;
    case SEARCH_SENSOR:
        res = msa31x_check();
        memcpy(arg, &res, 1);
        break;
    default:
        break;
    }
    return ret;
}


REGISTER_GRAVITY_SENSOR(gSensor) = {
    .logo = "msa310",
    .gravity_sensor_init  = msa31x_init,
    .gravity_sensor_check = NULL,
    .gravity_sensor_ctl   = msa31x_ctl,
};


#endif
