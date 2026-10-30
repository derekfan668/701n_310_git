
/*********************************************************************************************************
*               Copyright(c) 2018, Vcare Corporation. All rights reserved.
**********************************************************************************************************
* @file     module_heart_vc.c
* @brief
* @details
* @author
* @date
* @version  v1.6
*********************************************************************************************************
*/
#include "app_config.h"
#include "includes.h"
#include "hrSensor_manage.h"
#include "gSensor_manage.h"
#include "usr_timer.h"
#include "msg.h"
#include "vcHr11Hci.h"

#if defined(TCFG_VCHR11S_DEV_ENABLE) && TCFG_VCHR11S_DEV_ENABLE

#define LOG(...)           //dev_log(__VA_ARGS__)



#define DEV_W_ADDR         (0x33<<1 | 0)
#define DEV_R_ADDR         (0x33<<1 | 1)

/* 如果Gsensor数据读取代码已经添加，请将这个宏置1 */
#define ACCLE_EN           (1)
#define ACCLE_BUF_MAX      (25*2)

const uint32_t mcuOscData      = 30000;   // Timer clock frequency  Be used to adjust INT frequency
const uint16_t mcuI2cClock     = 400;     // MCU I2C clock frequency
const uint8_t vcHr11SampleRate = 25;      // Heart rate sample frequency

static bool     vcHr11IRQFlag = 0;
static vcHr11_t vcHr11;

static cbuffer_t *data_w_cbuf[2];
static u8  hrdata[600 * 2];

typedef struct {
    u8  workMode;
    u8  wearState;
    u8  maxLedCur;
    u8  psValue;	      //PS Sample value.
    u8  preValue[2];   //Environment Sample value.
    u8  envValue[3];	   //Environment Sample value.
    u8  ppgLength;
    u8  accelLength;
    u16 ppgValue[ACCLE_BUF_MAX * 2];	//PPG sample value.
    axis_info_t accel_buf[ACCLE_BUF_MAX];
} vcHrData_t;

static vcHrData_t vcHrData;

vcHr11Ret_t vcHr11WriteRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)
{
    hrsensor_write_nbyte(DEV_W_ADDR, startAddress, pRegisters, len);
    return VCHR11RET_ISOK;
}


vcHr11Ret_t vcHr11ReadRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len)
{
    hrsensor_read_nbyte(DEV_R_ADDR, startAddress, pRegisters, len);
    return VCHR11RET_ISOK;
}

u8 vcHr11CheckChipID(void)
{
    u8 id = 0;

    for (u8 i = 0; i < 5; i++) {
        vcHr11ReadRegisters(VCREG0, &id, VCREG0_SIZE);
        if (0x21 == id || 0x29 == id) {
            LOG("ok!");
            return true;
        }
        mdelay(10);
    }

    LOG("err! id=0x%02x", id);
    return false;
}


void vcHr11Start(vcHr11Mode_t vcHr11WorkMode)
{
    vcHr11_t *pVcHr11 = &vcHr11;

    LOG("mode=%d", vcHr11WorkMode);

    vcHr11SoftReset(pVcHr11);
    vcHr11StopSample(pVcHr11);

    memset(&vcHrData, 0, sizeof(vcHrData_t));
    vcHrData.workMode = vcHr11WorkMode;

    if (VCWORK_MODE_POWER_OFF != vcHr11WorkMode) {

        pVcHr11->vcSampleRate = vcHr11SampleRate;
        pVcHr11->mcuOscValue  = mcuOscData;
        pVcHr11->mcuSclRate   = mcuI2cClock;
        pVcHr11->workMode     = vcHr11WorkMode;

        vcHr11StartSample(pVcHr11);
    }
}

void vcHr11DumpRawData(void)
{
    u8 accLen  = vcHrData.accelLength;
    if (vcHr11.workMode == VCWORK_MODE_SPO2WORK) {
        u8 ppgLen  = vcHrData.ppgLength / 2;
        u8 dumpLen = accLen > ppgLen ? accLen : ppgLen;
        LOG("dumpLen=%d accLen=%d ppgLen=%d", dumpLen, accLen, ppgLen);
        for (u8 i = 0; i < dumpLen; i++) {
            LOG(" no%02d ppg: %d %d accel: %d %d %d", i + 1, vcHrData.ppgValue[i * 2], vcHrData.ppgValue[i * 2 + 1],
                vcHrData.accel_buf[i].x, vcHrData.accel_buf[i].y, vcHrData.accel_buf[i].z);
        }
    } else {
        u8 ppgLen  = vcHrData.ppgLength;
        u8 dumpLen = accLen > ppgLen ? accLen : ppgLen;
        LOG("dumpLen=%d accLen=%d ppgLen=%d", dumpLen, accLen, ppgLen);
        for (u8 i = 0; i < dumpLen; i++) {
            LOG(" no%02d ppg: %d %d accel: %d %d %d", i + 1, vcHrData.ppgValue[i], 0,
                vcHrData.accel_buf[i].x, vcHrData.accel_buf[i].y, vcHrData.accel_buf[i].z);
        }

    }
}

void vcHr11UpdateToMater(void)
{
    //小核800ms写入一次，主核1000ms读取一次，比较坏的情况会小核写2次，主核读一次
    if (cbuf_get_data_len(data_w_cbuf[0]) > sizeof(vcHrData_t)) {
        cbuf_clear(data_w_cbuf[0]);
        LOG("full");
    }
    cbuf_write(data_w_cbuf[0], &vcHrData, sizeof(vcHrData_t));
    // vcHr11DumpRawData();
    vcHrData.ppgLength   = 0;
    vcHrData.accelLength = 0;
}


int vcHr11Process(u32 *buf)
{
    u16 buf_len = 0;

    // LOG("sr=%d",vcHr11.vcSampleRate);
    if (vcHr11.vcSampleRate == vcHr11SampleRate) {
        u8 ppgLength = 0;
        vcHr11GetSampleValues(&vcHr11, &ppgLength);
        LOG("oscFlg=%d ppgLength=%d readFlg=%d  wearStatus=%d", vcHr11.oscCheckFinishFlag, ppgLength, vcHr11.vcFifoReadFlag, vcHr11.wearStatus);

        if (vcHr11.workMode == VCWORK_MODE_CROSSTALKTEST) {
            if (vcHrData.maxLedCur != vcHr11.sampleData.maxLedCur ||
                vcHrData.preValue[0] != vcHr11.sampleData.preValue[0] || vcHrData.preValue[1] != vcHr11.sampleData.preValue[1]) {
                vcHrData.maxLedCur    = vcHr11.sampleData.maxLedCur;
                vcHrData.preValue[0]  = vcHr11.sampleData.preValue[0];
                vcHrData.preValue[1]  = vcHr11.sampleData.preValue[1];
                LOG("CROSSTALKTEST: %d %d", vcHr11.sampleData.maxLedCur, vcHr11.sampleData.preValue[0]);
                vcHr11UpdateToMater();
            }
        } else if (vcHr11.vcFifoReadFlag && vcHr11.wearStatus) {
            vcHr11.vcFifoReadFlag = 0;

            if (ppgLength > 0) {
                memcpy(&vcHrData.ppgValue[vcHrData.ppgLength], vcHr11.sampleData.ppgValue, ppgLength * sizeof(u16));
            }

            vcHrData.wearState = vcHr11.wearStatus;
            vcHrData.workMode  = vcHr11.workMode;
            vcHrData.ppgLength = vcHrData.ppgLength + ppgLength;

            if (vcHr11.workMode == VCWORK_MODE_HRWORK && vcHrData.ppgLength >= 20 ||
                vcHr11.workMode == VCWORK_MODE_SPO2WORK && vcHrData.ppgLength >= 40) {
                vcHr11UpdateToMater();
            }

        } else if (vcHrData.wearState != vcHr11.wearStatus) {
            vcHrData.wearState = vcHr11.wearStatus;
            vcHrData.workMode  = vcHr11.workMode;
            vcHrData.ppgLength = 0;
            vcHr11UpdateToMater();
        }


    }
    return buf_len;
}


void vcHr11AccelDataInput(void *accel, u8 point)
{
#if ACCLE_EN
    if (vcHr11.workMode == VCWORK_MODE_HRWORK || vcHr11.workMode == VCWORK_MODE_SPO2WORK) {

        LOG("point=%d", point);
        u8 toal = vcHrData.accelLength + point;

        if (ACCLE_BUF_MAX > toal) {
            memcpy(&vcHrData.accel_buf[vcHrData.accelLength], accel, point * sizeof(axis_info_t));
            vcHrData.accelLength = toal;
        }
    }
#endif
}


extern u32 get_lp_time_cnt;
uint32_t vcHr11GetRtcCountFromMCU(void)
{
    LOG("timer_cnt = %d", get_lp_time_cnt);
    return get_lp_time_cnt;
}


u8 vcHr11PreInit(cbuffer_t  *hrcbuf, cbuffer_t  *spo2cbuf)
{
    if (!hrcbuf || !spo2cbuf) {
        ASSERT(0);
    }
    data_w_cbuf[0] = hrcbuf;
    data_w_cbuf[1] = spo2cbuf;

    cbuf_init(data_w_cbuf[0], hrdata, sizeof(hrdata));
    cbuf_init(data_w_cbuf[1], hrdata, sizeof(hrdata));

    return vcHr11CheckChipID();
}

int vcHr11IoCtl(u8 cmd, void *arg)
{
    // LOG("cmd=%d",cmd);

    int *data;
    int *msg = (int *)arg;
    int len;
    int ret = 0;

    switch (cmd) {
    case HR_SENSOR_ENABLE:
        vcHr11Start(VCWORK_MODE_HRWORK);
        break;
    case HR_SENSOR_DISABLE:
        vcHr11Start(VCWORK_MODE_POWER_OFF);
        break;
    case SPO2_SENSOR_ENABLE:
        vcHr11Start(VCWORK_MODE_SPO2WORK);
        break;
    case SPO2_SENSOR_DISABLE:
        vcHr11Start(VCWORK_MODE_POWER_OFF);
        break;
    case HR_SENSOR_LP_DETECTION:
        vcHr11Start(VCWORK_MODE_LPDETECTION);
        break;
    case HR_SENSOR_FACTORY_TEST:
        vcHr11Start(VCWORK_MODE_CROSSTALKTEST);
        break;

    case HR_SEARCH_SENSOR: {
        int valid = vcHr11CheckChipID();
        memcpy(arg, &valid, 4);
        break;
    }
    case HR_SENSOR_IRQ:
        ret = vcHr11Process(arg);
        break;

    default:
        break;
    }
    return ret;
}

REGISTER_HR_SENSOR(hrSensor) = {
    .logo = "vchr11s",
    .heart_rate_sensor_init  = vcHr11PreInit,
    .heart_rate_sensor_check = NULL,
    .heart_rate_sensor_ctl   = vcHr11IoCtl,
    .accel_data_input        = vcHr11AccelDataInput,
} ;

#endif