#include "app_config.h"
#include "includes.h"
#include "hrSensor_manage.h"
#include "gSensor_manage.h"
#include "usr_timer.h"
#include "hrs3602.h"
#include "hrs3600_reg_init.h"
#include "hrs3602_hrs_driv.h"

#if defined(TCFG_HRS3602_DEV_ENABLE) && TCFG_HRS3602_DEV_ENABLE

/* 如果Gsensor数据读取代码已经添加，请将这个宏置1 */
#define ACCLE_EN           (1)

typedef enum {
    MODE_POWER_OFF     = 0,
    MODE_LPDETECTION   = 1,
    MODE_HRWORK        = 2,
    MMODE_SPO2WORK      = 3,
} hrs3602_mode_t;

#define PPG_BUF_MAX         (25)
#define ACCLE_BUF_MAX       (25)

typedef struct {
    u8  ppg_point;
    u8  acc_point;
    u8  wear_status;
    u8  work_mode;
    axis_info_t acc_buff[ACCLE_BUF_MAX];
    s32 ppg_buff[PPG_BUF_MAX];
} hrs3602_raw_t;



static hrs3602_raw_t hrs3602_raw;
static cbuffer_t *data_w_cbuf[2];
static u8  cbuf_data[(4 + 6) * 25 * 2 + 100];

// sucess return 0   fail  return 1
bool Hrs3602_write_reg(u8 addr, u8 data)
{
    hrsensor_write_nbyte(0x88, addr, &data, 1);
    return 0;
}

u8 Hrs3602_read_reg(u8 addr)
{
    u8 data_buf = 0;
    hrsensor_read_nbyte(0x89, addr, &data_buf, 1);
    return data_buf;
}

bool Hrs3602_brust_read_reg(u8 addr, u8 *buf, u8 length)
{
    hrsensor_read_nbyte(0x89, addr, buf, length);
    return true;
}


void Hrs3602_read_data_packet()
{
    u8  databuf1[6] = {0};
    u8  databuf2[6] = {0};
    s32 P0 = 0, P1 = 0, P2 = 0, P3 = 0;

    Hrs3602_brust_read_reg(0xa0, databuf1, 6);
    Hrs3602_brust_read_reg(0xa6, databuf2, 6);

    P0 = ((databuf1[0]) | (databuf1[1] << 8) | (databuf1[2] << 16));
    P1 = ((databuf1[3]) | (databuf1[4] << 8) | (databuf1[5] << 16));
    P2 = ((databuf2[0]) | (databuf2[1] << 8) | (databuf2[2] << 16));
    P3 = ((databuf2[3]) | (databuf2[4] << 8) | (databuf2[5] << 16));

    Hrs3602_brust_read_reg(0xa0, databuf1, 6);
    Hrs3602_brust_read_reg(0xa6, databuf2, 6);
    //DEBUG_PRINTF("%d %d %d %d\r\n" ,P0,P1,P2,P3);

}

bool Hrs3602_read_hrs(s32 *hrm_data, s32 *als_data)
{
    u8  databuf[6] = {0};
    s32 P0 = 0, P1 = 0;

    Hrs3602_brust_read_reg(0xa0, databuf, 6);

    P0 = ((databuf[0]) | (databuf[1] << 8) | (databuf[2] << 16));
    P1 = ((databuf[3]) | (databuf[4] << 8) | (databuf[5] << 16));

    //DEBUG_PRINTF(0," %d %d \r\n" , P0, P1);
    if (P0 > P1) {
        *hrm_data = P0 - P1;
    } else {
        *hrm_data = 0;
    }
    *als_data = P0;

    return true;
}

bool Hrs3602_read_ps1(s32 *ppg_ir)
{
    u8  databuf[6] = {0};
    s32 P0 = 0, P1 = 0;

    Hrs3602_brust_read_reg(0xa6, databuf, 6);

    P0 = ((databuf[0]) | (databuf[1] << 8) | (databuf[2] << 16));
    P1 = ((databuf[3]) | (databuf[4] << 8) | (databuf[5] << 16));

    if (P0 > P1) {
        *ppg_ir = P0 - P1;
    } else {
        *ppg_ir = 0;
    }

    return true;
}

void Hrs3602_write_efuse()
{
    Hrs3602_write_reg(0x85, 0x20);
    Hrs3602_write_reg(0x7f, 0x10);
    Hrs3602_write_reg(0x80, 0x08);
    Hrs3602_write_reg(0x81, 0x44);
    Hrs3602_write_reg(0x82, 0x40);
    Hrs3602_write_reg(0x85, 0x00);
}


void Hrs3602_chip_disable(void)
{
    Hrs3602_write_reg(0x09, 0x03);
}

bool Hrs3602_check_chip_id(void)
{
    u8 chip_id = Hrs3602_read_reg(0x00);

    if (chip_id != 0x22) {
        LOG("err =%d", chip_id);
        return false;
    }
    LOG("sucess");
    return true;
}

bool Hrs3602_chip_init()
{
    if (Hrs3602_check_chip_id() == false) {
        LOG("chip id err!");
        return false;
    }

    for (u16 i = 0; i < INIT_ARRAY_SIZE; i++) {
        if (Hrs3602_write_reg(init_register_array[i][0], init_register_array[i][1]) != 0) {
            return false;
        }
    }

    return true;
}

void Hrs3602_alg_config(void)
{
    /*para init begin ....*/
    u32 prf_temp_clk_num = 0;    /*temperatrue phase clk num in each prf */
    u32 prf_hrs_clk_num = 0;     /*hrs phase clk num in each prf */
    u32 prf_ps_clk_num = 0;      /*ps phase clk num in each prf */
    u32 prf_wait_clk_num = 0;    /*wait time clk num in each prf */
    u32 en2rst_delay_clk_num = 0; /*en signal to first reset delay time clk num in each prf */
    u16 rst_clk_num = 0;
    u16 hrs_ckafe_clk_num = 0;
    u16 ps_ckafe_clk_num = 0;


    /*para init end ....*/

    /*chip config begin ....*/
    u16 sample_rate = 25;       /*config the data rate of chip frog2 ,uint is Hz*/
    u32 prf_clk_num = 2620000 / sample_rate; /*period in clk num, num = Fclk/fs */

    u8 temperature_enable = 0;  /*temperature test function enable  , 1 mean enable ; 0 mean disable */
    u8 hrs_enable = 1;             /*hrs function enable  , 1 mean enable ; 0 mean disable */
    u8 ps_enable = 1;             /*ps function enable  , 1 mean enable ; 0 mean disable */

    u8 temperature_adc_osr = 0; /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 ;*/
    u8 hrs_adc_osr = 3;           /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 ;*/
    u8 ps_adc_osr = 3;               /* 0 = 128 ; 1 = 256 ; 2 = 512 ; 3 = 1024 ;*/

    u8 led_dr_cfg = 3 ;  /* 0 = 12.5mA ; 1 = 25mA ; 2 =50mA(default) 3 = 100mA*/
    u8 tia_res = 2 ;     /* 0 = 54k(default) ; 1 = 108k ; 2 =216k 3 = 432k */

    en2rst_delay_clk_num = 64 ;   /* 0 ~ 127 clk num config */
    rst_clk_num = 6 ;             /* range from 0 to 7 ; 0=1 , 1=3, 2=7,....,7=255 */
    hrs_ckafe_clk_num = 128 ;     /* hrm phase led on time : 0~ 255 , ʵ�ʿ���10λ������λĬ��Ϊ0*/
    ps_ckafe_clk_num = 4 ;       /* ps phase led on time : 0~ 255 , ʵ�ʿ���10λ������λĬ��Ϊ0*/


    /*����PRF_WAIT��ʱ��*/
    if (temperature_enable == 1) {
        prf_temp_clk_num = (en2rst_delay_clk_num + 10) + 2 * ((2 << rst_clk_num) - 1) + 2 * (128 << temperature_adc_osr);
    }

    if (hrs_enable == 1) {
        prf_hrs_clk_num = (en2rst_delay_clk_num + 10) +
                          2 * (2 * ((2 << rst_clk_num) - 1) + (hrs_ckafe_clk_num << 4) + 10 + (128 << hrs_adc_osr));
    }

    if (ps_enable == 1) {
        prf_ps_clk_num = (en2rst_delay_clk_num + 10) +
                         2 * (2 * ((2 << rst_clk_num) - 1) + (ps_ckafe_clk_num << 4) + 10 + (128 << ps_adc_osr));
    }

    prf_wait_clk_num = prf_clk_num - prf_temp_clk_num - prf_hrs_clk_num - prf_ps_clk_num ;
    /*����PRF_WAIT��ʱ��*/

    Hrs3602_write_reg(0xc1, 0x08);   // config external pd as input  and cap mode enable
    Hrs3602_write_reg(0x03, 0x8f);   // all phase use same external pd

    /*register configration begin ....*/

    Hrs3602_write_reg(0x01, (temperature_enable << 2) | temperature_adc_osr);
    Hrs3602_write_reg(0x02, (hrs_enable << 2) | (hrs_adc_osr) | (ps_adc_osr << 4) | (ps_enable << 6));

    //DEBUG_PRINTF(0," reg_0x02 is %x \r\n" , Hrs3602_read_reg(0x02));


    Hrs3602_write_reg(0x04, hrs_ckafe_clk_num);
    Hrs3602_write_reg(0x05, ps_ckafe_clk_num);

    //DEBUG_PRINTF(0," %x %x \r\n" , Hrs3602_read_reg(0x04) , Hrs3602_read_reg(0x05));

    Hrs3602_write_reg(0x0a, prf_wait_clk_num);
    Hrs3602_write_reg(0x0b, prf_wait_clk_num >> 8);
    Hrs3602_write_reg(0x0c, prf_wait_clk_num >> 16);

    Hrs3602_write_reg(0x11, rst_clk_num);
    Hrs3602_write_reg(0x12, en2rst_delay_clk_num);

    //GPIO config
    Hrs3602_write_reg(0xc0, (0x84 | led_dr_cfg)); /* led dr config , 0x84 = 12.5mA , 0x85 = 25mA ,0x86 = 50 mA ,0x87 = 100mA */
    Hrs3602_write_reg(0xc2, (0x00 | tia_res));    /* tia res config , 0x00 = 54K , 0x01 = 108K ,0x02 = 216K ,0x03 = 432K */
    /*register configration end ....*/
    Hrs3602_write_reg(0x09, 0x02);

#ifdef IR_CHECK_TOUCH
    Hrs3602_write_reg(0X14, 0x80);
    Hrs3602_write_reg(0X15, 0x40);
#else
    Hrs3602_write_reg(0X14, 0x40);
    Hrs3602_write_reg(0X15, 0x40);
#endif

    Hrs3602_write_reg(0x08, 0x00);    // self clear int
}

void Hrs3602_int_clear()
{
    Hrs3602_write_reg(0x06, 0x7f);
}

void Hrs3602_proess(void *priv);
void Hrs3602_timer_start(u8 enable)
{
    static u16 Hrs3602_timer = 0;

    if (enable && Hrs3602_timer == 0) {
        Hrs3602_timer = usr_timer_add(NULL, Hrs3602_proess, 40, 0);
    } else if ((!enable) &&  Hrs3602_timer > 0) {
        usr_timer_del(Hrs3602_timer);
        Hrs3602_timer = 0;
    }
}


u8 Hrs3602_measure_start(void)
{
    if (Hrs3602_chip_init()) {
        Hrs3602_alg_config();
        Hrs3602_driv_init();

        hrs3602_raw.ppg_point   = 0;
        hrs3602_raw.acc_point   = 0;
        hrs3602_raw.wear_status = MSG_NO_TOUCH;
        hrs3602_raw.work_mode   = MODE_HRWORK;

        Hrs3602_timer_start(1);
        return 0;
    }
    return -1;
}

u8 Hrs3602_measure_stop(void)
{
    Hrs3602_timer_start(0);
    Hrs3602_chip_disable();
    hrs3602_raw.work_mode = MODE_POWER_OFF;
    return 0;
}


void Hrs3602_update_to_mater(void)
{
    LOG("point=%d %d wear=%d", hrs3602_raw.ppg_point, hrs3602_raw.acc_point, hrs3602_raw.wear_status);
    //小核800ms写入一次，主核1000ms读取一次，比较坏的情况会小核写2次，主核读一次
    if (cbuf_get_data_len(data_w_cbuf[0]) >= 2 * sizeof(hrs3602_raw_t)) {
        cbuf_clear(data_w_cbuf[0]);
        LOG("full");
    }
    cbuf_write(data_w_cbuf[0], &hrs3602_raw, sizeof(hrs3602_raw_t));
    // vcHr11DumpRawData();
    hrs3602_raw.ppg_point = 0;
    hrs3602_raw.acc_point = 0;
}

void Hrs3602_work_hr_mode(void)
{
    s32  ppg_green, ppg_env, ppg_ir;
    hrs3602_wear_status_t wear_status = MSG_NO_TOUCH;

    Hrs3602_read_hrs(&ppg_green, &ppg_env);
    Hrs3602_read_ps1(&ppg_ir);

    wear_status = Hrs3602_agc(ppg_env, ppg_ir);
    LOG(" gr=%d, als=%d ir=%d status=%d \r\n", ppg_green, ppg_env, ppg_ir, wear_status);

    if (wear_status == MSG_TOUCH) {
        if (hrs3602_raw.ppg_point < PPG_BUF_MAX) {
            hrs3602_raw.ppg_buff[hrs3602_raw.ppg_point] = ppg_green;
            hrs3602_raw.ppg_point++;
            if (hrs3602_raw.ppg_point >= 25) {
                Hrs3602_update_to_mater();
            }
        }
    }

    if (hrs3602_raw.wear_status != wear_status) {
        hrs3602_raw.wear_status = wear_status;
        if (wear_status == MSG_NO_TOUCH) {
            hrs3602_raw.ppg_point = 0;
            hrs3602_raw.acc_point = 0;
            Hrs3602_update_to_mater();
        }
    }

}

void Hrs3602_proess(void *priv)
{
    if (hrs3602_raw.work_mode == MODE_HRWORK) {
        Hrs3602_work_hr_mode();
    }
}

u8 Hrs3602_preInit(cbuffer_t  *hrcbuf, cbuffer_t  *spo2cbuf)
{
    if (!hrcbuf || !spo2cbuf) {
        ASSERT(0);
    }
    data_w_cbuf[0] = hrcbuf;
    data_w_cbuf[1] = spo2cbuf;

    cbuf_init(data_w_cbuf[0], cbuf_data, sizeof(cbuf_data));
    cbuf_init(data_w_cbuf[1], cbuf_data, sizeof(cbuf_data));

    return Hrs3602_check_chip_id();
}


void Hrs3602AccelDataInput(void *accel, u8 point)
{
    if (hrs3602_raw.work_mode == MODE_HRWORK) {

        if (accel != NULL && point > 0) {
            s16 free_point = ACCLE_BUF_MAX - hrs3602_raw.acc_point;
            if (free_point > 0) {
                point = MIN(point, free_point);
                memcpy(&hrs3602_raw.acc_buff[hrs3602_raw.acc_point], accel, point * sizeof(axis_info_t));
                hrs3602_raw.acc_point += point;
            }
        } else {
            LOG("err: accel=%d point=%d", accel, point);
        }
    }
}

s32 Hrs3602IoCtl(u8 cmd, void *arg)
{
    s32 len;
    s32 ret = 0;
    s32 *data;
    s32 *msg = (s32 *)arg;

    // LOG("cmd=%d",cmd);

    switch (cmd) {
    case HR_SENSOR_ENABLE:
        Hrs3602_measure_start();
        break;
    case HR_SENSOR_DISABLE:
        Hrs3602_measure_stop();
        break;
    case HR_SENSOR_LP_DETECTION:
        break;
    case HR_SENSOR_FACTORY_TEST:
        break;

    case HR_SEARCH_SENSOR: {
        int valid = Hrs3602_check_chip_id();
        memcpy(arg, &valid, 4);
        break;
    }
    default:
        break;
    }
    return ret;
}

REGISTER_HR_SENSOR(hrSensor) = {
    .logo = "hrs3602",
    .heart_rate_sensor_init  = Hrs3602_preInit,
    .heart_rate_sensor_check = NULL,
    .heart_rate_sensor_ctl   = Hrs3602IoCtl,
    .accel_data_input        = Hrs3602AccelDataInput,
} ;

#endif
