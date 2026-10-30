#include "app_config.h"
#include "vcHr11Hci.h"

#if defined(TCFG_VCHR11S_DEV_ENABLE) && TCFG_VCHR11S_DEV_ENABLE

static uint32_t lastRtcValue = 0;
static uint8_t ledCurrent[SLOTCHNU], ledMaxCurrent[SLOTCHNU] = {0x6f, 0x6f, 0x6f};
static uint8_t pdRes[SLOTCHNU], pdResMax[SLOTCHNU] = {7, 7, 7};
static uint8_t pdResSet[SLOTCHNU], ppgGain[SLOTCHNU];
static uint8_t recordEnv[SLOTCHNU] = {0, 0, 0}, pdBias = 0;
static uint16_t count_pdRes[SLOTCHNU], count_QaPdRes[SLOTCHNU];
static uint8_t adjustFlag[2][64], fifoRear = 0;
static uint8_t slotCount, sampleIndex, totalSlot = 0, readIndex = 0, readCnt = 0;
static uint8_t fifoWriteIndex = 0, fifoReadIndex = 0x80, fifoIntDiv = 0;
static uint8_t enFifo, enSlot[SLOTCHNU];
static uint16_t oscCnt = 0, cntCalOsc = 0;
static uint32_t intDiffRtc[8] = {0}, intDiffAdj[8] = {0};
static uint8_t oscDifSetting = 2;
static uint8_t fifoIntTim = (25 - 6);
static uint8_t PSIntTim = (24);	/* (n+1)*40=1sec */

extern int HeartRateValue;

#if SportMotionEn
int32_t gsensor_abs_xyz_sum_last_tick;
int32_t gsensor_abs_xyz_sum_current_tick;
int32_t gsensor_abs_xyz_diff_value;

/*
 * @brief
 *
 *
 **/
void green_led_off_state_gsensor_abs_sum_diff_init_func(void)
{
    gsensor_abs_xyz_sum_current_tick = 0;
    gsensor_abs_xyz_sum_last_tick = 0;
    gsensor_abs_xyz_diff_value = 0;
}

/*
 * @brief
 *
 *
 **/
void green_led_off_state_gsensor_abs_sum_diff_func(int32_t x_axis_value, int32_t y_axis_value, int32_t z_axis_value)
{
    int32_t x_axis_value_current = x_axis_value;
    int32_t y_axis_value_current = y_axis_value;
    int32_t z_axis_value_current = z_axis_value;

    gsensor_abs_xyz_sum_last_tick = gsensor_abs_xyz_sum_current_tick;
    if (x_axis_value < 0) {
        x_axis_value_current = -x_axis_value; // modify by Tatum
    }
    if (y_axis_value < 0) {
        y_axis_value_current = -y_axis_value;
    }
    if (z_axis_value < 0) {
        z_axis_value_current = -z_axis_value;
    }

    gsensor_abs_xyz_sum_current_tick = x_axis_value_current + y_axis_value_current + z_axis_value_current;
    if (gsensor_abs_xyz_sum_current_tick && gsensor_abs_xyz_sum_last_tick) {
        gsensor_abs_xyz_diff_value = gsensor_abs_xyz_sum_last_tick - gsensor_abs_xyz_sum_current_tick;
    } else {
        gsensor_abs_xyz_diff_value = 0;
    }
    if (gsensor_abs_xyz_diff_value < 0) {
        gsensor_abs_xyz_diff_value = -gsensor_abs_xyz_diff_value;
    }
}
#endif

/*
 * @brief
 * Function:Get the FifoIntDiv and slotNum Configuration
 *
 **/
static void vcHr11ConfigInit(vcHr11_t *pvcHr11)
{
    fifoIntDiv = (pvcHr11->regConfig[3] & 0x3f);
    enFifo = (fifoIntDiv != 0) ? 1 : 0;
    enSlot[0] = pvcHr11->regConfig[0] & 0x01;		 // Get Slot0 Status.
    enSlot[1] = (pvcHr11->regConfig[0] & 0x02) >> 1; // Get Slot1 Status.
    totalSlot = (pvcHr11->regConfig[0] & 0x03) == 3 ? 2 : ((pvcHr11->regConfig[0] & 0x03) == 0 ? 0 : 1);

    pvcHr11->adjustInfo[0].step = 4;
    pvcHr11->adjustInfo[0].directionLast = VCHR11ADJUSTDIRECTION_NULL;
    pvcHr11->adjustInfo[0].direction = VCHR11ADJUSTDIRECTION_NULL;

    pvcHr11->wearStatus = VCHR11WEARST_ISWEAR;

    pvcHr11->vcPsFlag = 0;
    pvcHr11->vcFifoReadFlag = 0;
    fifoRear = (pvcHr11->devId == 0x29) ? 32 : 64;

    pvcHr11->unWearCnt = VCUNWEAR_CNT;
    pvcHr11->isWearCnt = VCISWEAR_CNT;
    if (pvcHr11->devId == 0x29) {
        ledMaxCurrent[0] = 0x5f;
        ledMaxCurrent[1] = 0x5f;
    } else {
        ledMaxCurrent[0] = 0x6f;
        ledMaxCurrent[1] = 0x6f;
    }
    pdResMax[0] = 7;
    pdResMax[1] = 7;
    recordEnv[0] = 0;
    recordEnv[1] = 0;
}

/*
 * @brief
 * Function:Get the Dev ID And distinguish the chip version
 *
 **/
static vcHr11Ret_t vcHr11ReadVerison(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint8_t chipVersion = 0;
    uint8_t edition[17] = {0x84, 0x3F, 0x8A, 0x00, 0x07, 0xCF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x56, 0x16, 0x00};

    ret |= vcHr11WriteRegisters(VCREG11, edition, 17);

    ret |= vcHr11ReadRegisters(VCREG0, (uint8_t *)&pvcHr11->devId, VCREG0_SIZE);

    ret |= vcHr11ReadRegisters(VCREG1, (uint8_t *)&chipVersion, VCREG1_SIZE);

    pvcHr11->version = ((chipVersion & 0x04) != 0) ? VERSION_A : VERSION_B;

    vcHr11SoftReset(pvcHr11);
    return ret;
}

/*
 * @brief
 * Function:Initialize vcHr11's register Settings
 *
 **/
static vcHr11Ret_t vcHr11RegInit(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    uint8_t i = 0;
    uint8_t regConfig[17] = {0x01, 0x27, 0x8A, 0x40, 0x03, 0x1F, 0x00, 0x00, 0x80, 0x4A,
                             0x57, 0x37, 0x47,
                             0x16, 0x56, 0x16, 0x00
                            };
    fifoIntTim = pvcHr11->vcSampleRate - 6;
    regConfig[3] = (fifoIntTim) | regConfig[3];
    switch (pvcHr11->workMode) {
    case VCWORK_MODE_HRWORK:
        regConfig[0] = 0x05;
#if HW_HALVE_POWER_CAP
        regConfig[10] = 0x54;
        regConfig[13] = 0x03; //
        regConfig[16] = 0x01;
#endif
        break;

    case VCWORK_MODE_SPO2WORK:
        regConfig[0] = 0x07;
        regConfig[1] = 0x2F;
        regConfig[6] = 0;
        regConfig[7] = 0x80;
        regConfig[13] = 0x96;
        regConfig[16] = 0x04; // OverSample
        break;

    case VCWORK_MODE_CROSSTALKTEST:
        regConfig[0] = 0x05;
        regConfig[1] = 0x3F;
#if !HW_HALVE_POWER_CAP
        regConfig[7] = 0xEF;
        regConfig[10] = 0x77;
#else
        regConfig[7] = 0xEF;  // 0xDF;
        regConfig[10] = 0x74; // 0x77;
        regConfig[13] = 0x03; // 0x16
        regConfig[16] = 0x01;
#endif
        break;

    case VCWORK_MODE_NOISEWORK:
        regConfig[0] = 0x05;
        regConfig[10] = 0x57;
        regConfig[13] = 0x16;
        regConfig[16] = 0x00;
#if HW_HALVE_POWER_CAP
        regConfig[10] = 0x54;
        regConfig[13] = 0x03;
        regConfig[16] = 0x01;
#endif
        break;

    case VCWORK_MODE_LPDETECTION:
        regConfig[0] = 0x04;
        regConfig[1] = 0x30;
        regConfig[6] = PSIntTim;
#if HW_HALVE_POWER_CAP
        regConfig[10] = 0x54;
        regConfig[13] = 0x03;
        regConfig[16] = 0x01;
#endif
        break;

    default:
        break;
    }

    if (pvcHr11->devId == 0x29) {
        // Since VC010 has only one PPG channel, the fifo depth is reduced from the original 64 sampling points to 32 sampling points.
        // Set the B5 position of fifointdiv to 0 on the driver to reduce the depth of fifo.
        regConfig[3] = 0xDF & regConfig[3]; // set B5 to 0 reduce the Fifo's length
        regConfig[9] = 0x4A;
        regConfig[10] = 0x5E;
        regConfig[13] = 0x3C;
#if HW_HALVE_POWER_CAP
        regConfig[10] = 0x57;
        regConfig[13] = 0x16;
        regConfig[16] = 0x01;
#endif
        if (pvcHr11->workMode == VCWORK_MODE_CROSSTALKTEST) {
            regConfig[7] = 0xDF;
        }
        /*******************************
         * HW_LC09A_PROJ,20220915
         *******************************/
#if HW_LC09A_PROJ
        regConfig[9]  = 0xD0;/* HS+0xD0(40mA) */
        regConfig[12] = 0x77;
#if 1	/* 100us */
        regConfig[10] = 0x57;
        regConfig[13] = 0x96;
#else	/* 200us */
        regConfig[10] = 0x5E;
        regConfig[13] = 0xBC;
#endif
        regConfig[16] = 0x00;
#endif
    }
    for (i = 0; i < 17; i++) {
        pvcHr11->regConfig[i] = regConfig[i];
        DEBUG_LOG("regConfig[%d]=0x%2x", i, pvcHr11->regConfig[i]);
    }
    ret |= vcHr11WriteRegisters(VCREG11, pvcHr11->regConfig, 17);
    pvcHr11->regConfig[0] = 0x80 | regConfig[0];
    ret |= vcHr11WriteRegisters(VCREG11, pvcHr11->regConfig, 1);
    vcHr11ConfigInit(pvcHr11);

    return ret;
}

/*
 * @brief
 * Function:
 *
 **/
vcHr11Ret_t vcHr11StartSample(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    ret |= vcHr11ReadVerison(pvcHr11);
    /* Add the delay */
    ret |= vcHr11RegInit(pvcHr11);

    return ret;
}

/*
 * @brief
 * Function: shut down the logic channel
 *            workmode = sleepmode
 *
 **/
vcHr11Ret_t vcHr11StopSample(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint8_t modeCtrl = 0x00; // Shut down all logic solt and goto sleep mode
    ret |= vcHr11WriteRegisters(VCREG11, (uint8_t *)&modeCtrl, VCREG11_SIZE);
    return ret;
}

/*
 * @brief
 * Function: Added adjust flag when interrupt happend.
 *            Provides it for algorithmic processing.
 *
 **/
static vcHr11Ret_t vcHr11AddAdjustFlag(uint8_t slotNum)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    /* If FIFOIntDiv equal to 0, FIFO Function isn't enabled */
    if (fifoIntDiv == 0) {
        adjustFlag[slotNum][0] = 1;
    } else {
        ret |= vcHr11ReadRegisters(VCREG3, (uint8_t *)&fifoWriteIndex, VCREG3_SIZE);
        sampleIndex = (fifoWriteIndex - 0x80) / 2 / totalSlot;
        if (sampleIndex == 0) {
            // sampleIndex = totalSlot == 1 ? 63 : 31;
            sampleIndex = totalSlot == 1 ? fifoRear - 1 : (fifoRear / 2) - 1;
        } else {
            sampleIndex = sampleIndex - 1;
        }
        adjustFlag[slotNum][sampleIndex] = 1;
    }
    return ret;
}

/*
 * @brief
 * Function: Read data from FIFO
 *            If there is an adjustment, an adjustment flag is added
 *
 **/
static vcHr11Ret_t vcHr11OutPutPPGData(vcHr11_t *pvcHr11, uint8_t StartIndex, uint16_t StopIndex, uint8_t IndexFlag)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint16_t i = 0;
    uint8_t dataLength = 0;
    uint8_t sampleData[128];

    dataLength = StopIndex - StartIndex;
    ret |= vcHr11ReadRegisters(StartIndex, sampleData, dataLength);

    for (i = StartIndex; i < StopIndex; i = i + 2) { // If i set to uint8_t and i = 254,i will overflow
        pvcHr11->sampleData.ppgValue[(i - StartIndex) / 2 + IndexFlag] = ((sampleData[(i - StartIndex)] << 8) | sampleData[(i - StartIndex) + 1]);
        slotCount = ((i - 0x80) % (totalSlot * 2)) / 2;
        sampleIndex = (i - 0x80) / (totalSlot * 2);
        if (adjustFlag[slotCount][sampleIndex] == 1) {
            pvcHr11->sampleData.ppgValue[(i - StartIndex) / 2 + IndexFlag] |= 0x1000;
            adjustFlag[slotCount][sampleIndex] = 0;
        }
    }
    return ret;
}

/*
 * @brief
 * Function:Clock Calibration
 *
 **/
uint16_t vcHr11CalculateOSCFreq(vcHr11_t *pvcHr11)
{

    uint8_t clkAdjCnt[4] = {0}, newFreDiv[2] = {0}, i = 0;
    // uint8_t vcAdj_Th,vcRtc_Th,
    uint32_t rtcCount[2];
    uint16_t adjCount[2], freDiv[2] = {0};
    uint16_t newDiv, f20kOsc;

    bool diffCountIsOk = true;
    bool diffIsOK;

    rtcCount[0] = vcHr11GetRtcCountFromMCU(); // Get McuRtcCount
    vcHr11ReadRegisters(VCREG7, clkAdjCnt, 4);
    rtcCount[1] = vcHr11GetRtcCountFromMCU(); // Get McuRtcCount

    adjCount[0] = clkAdjCnt[0] << 8 | clkAdjCnt[1];
    adjCount[1] = clkAdjCnt[2] << 8 | clkAdjCnt[3];

    if (adjCount[1] < adjCount[0] || rtcCount[1] < rtcCount[0]) {
        cntCalOsc = 0;
        return 0;
    }

    intDiffAdj[2 * cntCalOsc] = adjCount[0];
    intDiffAdj[2 * cntCalOsc + 1] = adjCount[1];

    intDiffRtc[2 * cntCalOsc] = rtcCount[0];
    intDiffRtc[2 * cntCalOsc + 1] = rtcCount[1];

    cntCalOsc += 1;

    if (cntCalOsc >= 3) {
        cntCalOsc = 0;
        for (i = 0; i < 2; i++) {
            diffCountIsOk = (((intDiffRtc[2 * i + 2] - intDiffRtc[2 * i]) - (intDiffRtc[2 * i + 3] - intDiffRtc[2 * i + 1]) <= oscDifSetting) || ((intDiffRtc[2 * i + 3] - intDiffRtc[2 * i + 1]) - (intDiffRtc[2 * i + 2] - intDiffRtc[2 * i]) <= oscDifSetting)) && diffCountIsOk;
            diffCountIsOk = (((intDiffAdj[2 * i + 2] - intDiffAdj[2 * i]) - (intDiffAdj[2 * i + 3] - intDiffAdj[2 * i + 1]) <= 2) || ((intDiffAdj[2 * i + 3] - intDiffAdj[2 * i + 1]) - (intDiffAdj[2 * i + 2] - intDiffAdj[2 * i]) <= 2)) && diffCountIsOk;
        }
        if (diffCountIsOk) {
            for (i = 0; i < 2; i++) {
                if (intDiffRtc[2 * i + 2] == intDiffRtc[2 * i]) {
                    cntCalOsc = 0;
                    return 0;
                }
                freDiv[i] = ((uint32_t)(intDiffAdj[2 * i + 2] - intDiffAdj[2 * i])) * pvcHr11->mcuOscValue / pvcHr11->vcSampleRate / ((uint32_t)(intDiffRtc[2 * i + 2] - intDiffRtc[2 * i])) - 1;
                if (freDiv[i] < (20000 / pvcHr11->vcSampleRate * 0.75f) || freDiv[i] > (20000 / pvcHr11->vcSampleRate * 1.25f)) {
                    return 0;
                }
            }

            diffIsOK = (freDiv[1] - freDiv[0] <= 20) || (freDiv[0] - freDiv[1] <= 20);
            if (diffIsOK) {
                newDiv = (freDiv[1] + freDiv[0]) / 2;
                f20kOsc = (newDiv + 1) * pvcHr11->vcSampleRate;
                newFreDiv[0] = newDiv >> 8;
                newFreDiv[1] = newDiv;
                vcHr11WriteRegisters(VCREG15, newFreDiv, VCREG15_SIZE);
                pvcHr11->oscCheckFinishFlag = 1;
                oscCnt = 0;
            }
        } else {
            return 0;
        }
    }
    pvcHr11->oscCheckFinishFlag = 1; //强制校准通过
    return f20kOsc;
}

/*
 * @brief
 * Function: Wear monitoring.
 *
 **/
static vcHr11Ret_t vcHr11WearStatusDetection(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint8_t newSoltChNu = 0;
    uint8_t enPs = (pvcHr11->regConfig[1] & 0xf0) | 0x10;
    switch (pvcHr11->wearStatus) {
    // Identify  whether the wearing state needs to be switched
    // Wear status from unWear to isWear
    case VCHR11WEARST_UNWEAR:
#if SportMotionEn
        if ((pvcHr11->sampleData.psValue >= VCPS_TH) && (pvcHr11->sampleData.envValue[2] < 3) && (gsensor_abs_xyz_diff_value > 20))
#else
        if ((pvcHr11->sampleData.psValue >= VCPS_TH) && (pvcHr11->sampleData.envValue[2] < 3))
#endif
        {
            if (--pvcHr11->isWearCnt <= 0) { // Once time
                pvcHr11->wearStatus = VCHR11WEARST_ISWEAR;
                pvcHr11->unWearCnt = VCUNWEAR_CNT;
                pvcHr11->isWearCnt = VCISWEAR_CNT;

                vcHr11SoftReset(pvcHr11); // Reset Reg to defult value
                vcHr11RegInit(pvcHr11);	  //
                ret |= VCHR11RET_UNWEARTOISWEAR;
                lastRtcValue = vcHr11GetRtcCountFromMCU();
#if SportMotionEn
                green_led_off_state_gsensor_abs_sum_diff_init_func();
#endif
            }
        } else {
            pvcHr11->isWearCnt = VCISWEAR_CNT;
        }
        break;

    case VCHR11WEARST_ISWEAR:
#if BiometricEn
        if ((pvcHr11->sampleData.psValue < VCPS_TH) || (pvcHr11->sampleData.envValue[2] > VCENV_TH) || (HeartRateValue == -1)) {
            if (HeartRateValue == -1) {
                pvcHr11->unWearCnt = 1;
            }
            if (--pvcHr11->unWearCnt <= 0) { // 3 times.
                HeartRateValue = 0;
                pvcHr11->wearStatus = VCHR11WEARST_UNWEAR;
                pvcHr11->unWearCnt = VCUNWEAR_CNT;
                pvcHr11->isWearCnt = VCISWEAR_CNT;
                newSoltChNu = (pvcHr11->regConfig[0] & 0xF8) | 0x04; // Enable solt2,Disable solt0 and solt1;
                pvcHr11->vcPsFlag = 1;
                vcHr11WriteRegisters(VCREG16, (uint8_t *)&PSIntTim, VCREG16_SIZE);
                vcHr11WriteRegisters(VCREG12, (uint8_t *)&enPs, VCREG12_SIZE);
                pvcHr11->regConfig[1] |= 0x10;
                ret |= vcHr11WriteRegisters(VCREG11, (uint8_t *)&newSoltChNu, VCREG11_SIZE);
                ret |= VCHR11RET_ISWEARTOUNWEAR;
            }
        }
#else
        if ((pvcHr11->sampleData.psValue < VCPS_TH) || (pvcHr11->sampleData.envValue[2] > VCENV_TH)) {
            if (--pvcHr11->unWearCnt <= 0) { // 3 times.
                pvcHr11->wearStatus = VCHR11WEARST_UNWEAR;
                pvcHr11->unWearCnt = VCUNWEAR_CNT;
                pvcHr11->isWearCnt = VCISWEAR_CNT;
                newSoltChNu = (pvcHr11->regConfig[0] & 0xF8) | 0x04; // Enable solt2,Disable solt0 and solt1;
                pvcHr11->vcPsFlag = 1;
                vcHr11WriteRegisters(VCREG16, (uint8_t *)&PSIntTim, VCREG16_SIZE);
                vcHr11WriteRegisters(VCREG12, (uint8_t *)&enPs, VCREG12_SIZE);
                pvcHr11->regConfig[1] |= 0x10;
                ret |= vcHr11WriteRegisters(VCREG11, (uint8_t *)&newSoltChNu, VCREG11_SIZE);
                ret |= VCHR11RET_ISWEARTOUNWEAR;
            }
        }
#endif
        else {
            pvcHr11->vcPsFlag = 0;
            pvcHr11->unWearCnt = VCUNWEAR_CNT;
        }
        break;
    }
    return ret;
}

/*
 * @brief
 * Function: FIFO Read.(Or GetValue)
 *
 **/
static vcHr11Ret_t vcHr11ReadFifo(vcHr11_t *pvcHr11, uint8_t *len)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint32_t rtcValue = 0;
    uint8_t dateLength = 0;
    uint8_t dataBias = 0;
    uint8_t dataOverFlow = 0;
    uint16_t fifoEnd = 0;
    float cacheTime = 0;

    cacheTime = (pvcHr11->devId == 0x29) ? 32.0f : 64.0f;

    dataBias = (pvcHr11->devId == 0x29) ? 0x40 : 0x80;

    dataOverFlow = (pvcHr11->devId == 0x29) ? 0x20 : 0x40;

    fifoEnd = (pvcHr11->devId == 0x29) ? 192 : 256;

    ret |= vcHr11ReadRegisters(VCREG3, (uint8_t *)&fifoWriteIndex, VCREG3_SIZE);

    rtcValue = vcHr11GetRtcCountFromMCU();
    /*
    	uint32_t vcHr02TimeDiff;
     rtcValue = vcHr02GetRtcCountFromMCU();
    	if (rtcValue > lastRtcValue)
    	{
    			vcHr02TimeDiff = rtcValue - lastRtcValue;
    	}
    	else
    	{
    			vcHr02TimeDiff = (0xFFFFFF - lastRtcValue) + rtcValue;
    	}
    	if((vcHr02TimeDiff)>(((cacheTime/pvcHr02->vcSampleRate) *0.8f* pvcHr02->mcuOscValue)/ totalSlot))
    */
    if ((rtcValue - lastRtcValue) > (((cacheTime / pvcHr11->vcSampleRate) * 0.8f * pvcHr11->mcuOscValue) / totalSlot)) {
        pvcHr11->vcFifoReadFlag = 0;
        fifoWriteIndex = 0;
        fifoReadIndex = 0x80;
        lastRtcValue = rtcValue;
        return VCHR11RET_FIFOISOVERFLOW;
    }

    if (fifoIntDiv != 0) { // If fifoIntDiv equal to 0, FIFO Function is not enabled
        if (fifoWriteIndex > fifoReadIndex) {
            dateLength = (fifoWriteIndex - fifoReadIndex) / 2;
            vcHr11OutPutPPGData(pvcHr11, fifoReadIndex, fifoWriteIndex, 0);
        } else {
            dateLength = (fifoWriteIndex + dataBias - fifoReadIndex) / 2;

            if (dateLength == dataOverFlow) {
                dateLength = 0;
                pvcHr11->vcFifoReadFlag = 0;
                return VCHR11RET_ISERR;
            }
            vcHr11OutPutPPGData(pvcHr11, fifoReadIndex, fifoEnd, 0);
            if (fifoWriteIndex != 0x80) {
                vcHr11OutPutPPGData(pvcHr11, 0x80, fifoWriteIndex, (fifoEnd - fifoReadIndex) / 2);
            }
            /*if(pvcHr11->devId == 0x29)
            {
            	DBG_DIRECT("INfifoReadIndex is: %d,INfifoWriteIndex is: %d\n", fifoReadIndex,fifoWriteIndex);
            	//vcHr11OutPutPPGData(pvcHr11,fifoReadIndex,fifoWriteIndex + dataBias,0);
            	vcHr11OutPutPPGData(pvcHr11,fifoReadIndex,192,0);
            	if(fifoWriteIndex != 0x80)
            	{
            		vcHr11OutPutPPGData(pvcHr11,0x80,fifoWriteIndex,(192-fifoReadIndex)/2);
            	}
            }
            else
            {
            	vcHr11OutPutPPGData(pvcHr11,fifoReadIndex,256,0);
            	if(fifoWriteIndex != 0x80)
            	{
            		vcHr11OutPutPPGData(pvcHr11,0x80,fifoWriteIndex,(256-fifoReadIndex)/2);
            	}
            }*/
        }
        fifoReadIndex = fifoWriteIndex;
    } else {
        // Not use FIFO, the sampleData is storage in 0x80
        dateLength = totalSlot;
        vcHr11OutPutPPGData(pvcHr11, fifoReadIndex, fifoReadIndex + totalSlot * 2, 0);
    }
    // Return the array length for data processing
    *len = dateLength;
    lastRtcValue = rtcValue;
    return ret;
}

/*
 * @brief
 * Function: Adjust PDRes Value and modify the PDResMax
 *
 **/
static vcHr11Ret_t vcHr11EnvAdjust(vcHr11_t *pvcHr11, uint8_t slotNum)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    bool pdBiasFlag = false;

    if ((pvcHr11->sampleData.envValue[slotNum] > VCENV_TH) && enSlot[slotNum]) {
        recordEnv[slotNum] = pvcHr11->sampleData.envValue[slotNum];
        if (pvcHr11->devId == 0x29) {
            if (pdBias <= 15) {
                pdBias += 1;
                vcHr11AddAdjustFlag(slotNum); // Added logical adjustment flag bit
                pdBiasFlag = true;
                ret |= vcHr11WriteRegisters(VCREG28, (uint8_t *) & (pdBias), 1);
            }
        }
        // If pdBias cannot resist ambient light, we can choose to sacrifice the skin level to reduce the pdRes
        if (pdBiasFlag == false) {
            if (pdRes[slotNum] != 0) {
                pdRes[slotNum] = pdRes[slotNum] - 1;
                pdResMax[slotNum] = pdRes[slotNum];
                vcHr11AddAdjustFlag(slotNum); // Added logical adjustment flag bit
                pvcHr11->regConfig[slotNum + 10] = (pdRes[slotNum] << 4) | pdResSet[slotNum];
                ret |= vcHr11WriteRegisters((VCREG20 + slotNum), (uint8_t *) & (pvcHr11->regConfig[slotNum + 10]), 1);
            } else {
                return VCHR11RET_ENVCURRENTISLARGE;
            }
        }
    }
    if (pvcHr11->sampleData.envValue[2] > VCENV_TH) {
        return VCHR11RET_PSENVCURRENTISLARGE;
    }
    return ret;
}

/*
 * @brief
 * Function: Adjust Ledcurrent Value and modify the ledcurrentMax value
 *
 **/
static vcHr11Ret_t vcHr11OvloadAdjust(vcHr11_t *pvcHr11, uint8_t slotNum)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint8_t overLoad = 0;
    uint8_t ovload[SLOTCHNU];
    ret |= vcHr11ReadRegisters(VCREG1, (uint8_t *)&overLoad, VCREG1_SIZE);
    // Get Slot0,Slot1 and Slot2's Overload status.
    ovload[0] = overLoad & 0x01;
    ovload[1] = (overLoad & 0x02) >> 1;
    ovload[2] = (overLoad & 0x04) >> 2;
    // Adjust led emitter and record max_led_emitter.
    if ((overLoad & (pvcHr11->regConfig[0] & 0x07)) != 0) {
        if (ovload[slotNum] && enSlot[slotNum]) {
            if (ledCurrent[slotNum] != 0) {
                ledCurrent[slotNum] = ledCurrent[slotNum] - 1;
                ledMaxCurrent[slotNum] = ledCurrent[slotNum];
                vcHr11AddAdjustFlag(slotNum);
                pvcHr11->regConfig[slotNum + 7] = (ledCurrent[slotNum] | ppgGain[slotNum]);
                ret |= vcHr11WriteRegisters((VCREG17 + slotNum), (uint8_t *) & (pvcHr11->regConfig[slotNum + 7]), 1);
            } else {
                return VCHR11RET_SOLTISOVERLOAD;
            }
        }
        if (ovload[2] && pvcHr11->version != VERSION_A) { // After the calibration of infrared channel parameters, the current and resistance will not be adjusted
            return VCHR11RET_PSISOVERLOAD;
        }
    }
    return ret;
}

/*
 * @brief
 * Function: Adjust Ledcurrent Value and PDRes value
 *
 **/
static vcHr11Ret_t vcHr11PPgLedCurAndPdResAdjust(vcHr11_t *pvcHr11, uint8_t slotNum, uint8_t ledcur, uint8_t pdres)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    uint8_t oldPdRes, oldLedCurrent;
    uint8_t newPdRes, newLedCurrent;

    oldLedCurrent = ledCurrent[slotNum];
    newLedCurrent = oldLedCurrent;
    oldPdRes = pdRes[slotNum];
    newPdRes = oldPdRes;

    if ((oldLedCurrent == ledcur) && (pvcHr11->workMode != VCWORK_MODE_NOISEWORK)) {
        if (oldPdRes == pdres) {
            newPdRes = oldPdRes;
            return VCHR11RET_PPGCANNOTADJUSTABLE;
        } else {
            if (pvcHr11->adjustInfo[slotNum].direction == VCHR11ADJUSTDIRECTION_UP) {
                newPdRes = (oldPdRes >= 7) ? 7 : (oldPdRes + 1);
            } else {
                newPdRes = (oldPdRes < 1) ? 0 : (oldPdRes - 1);
            }
            newLedCurrent = oldLedCurrent;
            vcHr11AddAdjustFlag(slotNum);
        }
    } else {
        if (pvcHr11->adjustInfo[slotNum].directionLast == VCHR11ADJUSTDIRECTION_NULL) {
            pvcHr11->adjustInfo[slotNum].step *= 16;
        } else if (pvcHr11->adjustInfo[slotNum].direction == pvcHr11->adjustInfo[slotNum].directionLast) {
            if ((pvcHr11->adjustInfo[slotNum].step == 1) || (pvcHr11->adjustInfo[slotNum].step == 2)) {
                pvcHr11->adjustInfo[slotNum].step = (pvcHr11->adjustInfo[slotNum].step + 1) * 16;
            } else {
                pvcHr11->adjustInfo[slotNum].step *= VCADJUST_INCREASE;
            }
        } else {
            pvcHr11->adjustInfo[slotNum].step *= VCADJUST_DECREASE;
        }

        pvcHr11->adjustInfo[slotNum].step = pvcHr11->adjustInfo[slotNum].step >> 4;

        pvcHr11->adjustInfo[slotNum].step = (pvcHr11->adjustInfo[slotNum].step <= VCADJUST_STEP_MIN) ? VCADJUST_STEP_MIN : pvcHr11->adjustInfo[slotNum].step;
        pvcHr11->adjustInfo[slotNum].step = (pvcHr11->adjustInfo[slotNum].step >= VCADJUST_STEP_MAX) ? VCADJUST_STEP_MAX : pvcHr11->adjustInfo[slotNum].step;

        if (pvcHr11->adjustInfo[slotNum].direction == VCHR11ADJUSTDIRECTION_UP) {
            newLedCurrent = ((oldLedCurrent + pvcHr11->adjustInfo[slotNum].step) > ledcur) ? ledcur : oldLedCurrent + pvcHr11->adjustInfo[slotNum].step;
        } else {
            newLedCurrent = (oldLedCurrent < pvcHr11->adjustInfo[slotNum].step) ? ledcur : oldLedCurrent - pvcHr11->adjustInfo[slotNum].step;
        }
        vcHr11AddAdjustFlag(slotNum);
        newPdRes = oldPdRes;
        pvcHr11->adjustInfo[slotNum].directionLast = pvcHr11->adjustInfo[slotNum].direction;
    }

    ledCurrent[slotNum] = newLedCurrent;
    pdRes[slotNum] = newPdRes;

    pvcHr11->regConfig[slotNum + 7] = ledCurrent[slotNum] | ppgGain[slotNum];
    ret |= vcHr11WriteRegisters((VCREG17 + slotNum), (uint8_t *) & (pvcHr11->regConfig[slotNum + 7]), 1);
    pvcHr11->regConfig[slotNum + 10] = (pdRes[slotNum] << 4) | pdResSet[slotNum];
    ret |= vcHr11WriteRegisters((VCREG20 + slotNum), (uint8_t *) & (pvcHr11->regConfig[slotNum + 10]), 1);

    return ret;
}

/*
 * @brief
 * Function: Get the PPG saturation direction
 *
 **/
static vcHr11Ret_t vcHr11PpgAdjust(vcHr11_t *pvcHr11, uint8_t slotNum)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    uint8_t ppgDataAddress, ppgDataAddress_EnFifo;
    uint8_t ppgDate[2];
    uint16_t ppgCurValue[2];
    uint8_t IndexGap;
    // Get FIFOWriteIndex Value.
    ret |= vcHr11ReadRegisters(VCREG3, (uint8_t *)&fifoWriteIndex, VCREG3_SIZE);

    if (totalSlot == 1) {
        ppgDataAddress_EnFifo = fifoWriteIndex - totalSlot * 2; // Determine whether the FIFO is overflowing
        ppgDataAddress_EnFifo = ppgDataAddress_EnFifo < 0x80 ? ppgDataAddress_EnFifo + 0x80 : ppgDataAddress_EnFifo;
        ppgDataAddress = enFifo ? ppgDataAddress_EnFifo : 0x80;
    } else {
        ppgDataAddress_EnFifo = fifoWriteIndex - totalSlot * 2 + slotNum * 2; // Determine whether the FIFO is overflowing
        ppgDataAddress_EnFifo = ppgDataAddress_EnFifo < 0x80 ? ppgDataAddress_EnFifo + 0x80 : ppgDataAddress_EnFifo;
        ppgDataAddress = enFifo ? ppgDataAddress_EnFifo : 0x80 + slotNum * 2;
    }
    ret |= vcHr11ReadRegisters(ppgDataAddress, ppgDate, 2);
    ppgCurValue[slotNum] = (ppgDate[0] << 8) | ppgDate[1];

    if (fifoIntDiv != 0) {
        IndexGap = (fifoWriteIndex > readIndex) ? (fifoWriteIndex - readIndex) : (fifoWriteIndex - readIndex + 0x80);
        if (IndexGap != 2 * totalSlot) {
            pvcHr11->adjustInfo[slotNum].directionLast = VCHR11ADJUSTDIRECTION_NULL;
        }
        readCnt += 1;
        if (readCnt == totalSlot) {
            readCnt = 0;
            readIndex = fifoWriteIndex;
        }
    }
    // PPG supersaturation
    if (ppgCurValue[slotNum] > 4095 - VCPPG_TH * 32) {
        // If the LED luminous current reaches the maximum current,
        // it can only be adjusted by increasing the PD resistance
        pvcHr11->adjustInfo[slotNum].direction = VCHR11ADJUSTDIRECTION_UP;
        ret |= vcHr11PPgLedCurAndPdResAdjust(pvcHr11, slotNum, ledMaxCurrent[slotNum], pdResMax[slotNum]);
    }
    //	Lower saturation
    else if (ppgCurValue[slotNum] < VCPPG_TH * 32) {
        pvcHr11->adjustInfo[slotNum].direction = VCHR11ADJUSTDIRECTION_DOWN;
        ret |= vcHr11PPgLedCurAndPdResAdjust(pvcHr11, slotNum, 0, 0);
    }
    return ret;
}

/*
 * @briefs
 * Function: Prejude the ENVvalue and modify the PD maximum resistance.
 *            Don't need to write to register
 *
 **/
static vcHr11Ret_t vcHr11AdjustPDResMax(vcHr11_t *pvcHr11, uint8_t slotNum)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    uint8_t minEnv[SLOTCHNU];

    if ((pdResMax[slotNum] == 7) && (pdBias == 0)) {
        return VCHR11RET_PDRESISMAX;
    }
    minEnv[slotNum] = ((recordEnv[slotNum] / 2) < VCENV_TH) ? (recordEnv[slotNum] / 2) : VCENV_TH;
    if (pvcHr11->sampleData.envValue[slotNum] < minEnv[slotNum]) {
        if (pvcHr11->sampleData.envValue[slotNum] < 1) {
            count_pdRes[slotNum] = 0;
            count_QaPdRes[slotNum] = (count_QaPdRes[slotNum] == 30 * pvcHr11->vcSampleRate) ? 0 : count_QaPdRes[slotNum] + 1;
            pdResMax[slotNum] = (count_QaPdRes[slotNum] == 30 * pvcHr11->vcSampleRate) ? 7 : pdResMax[slotNum];
            pdBias = (count_QaPdRes[slotNum] == 30 * pvcHr11->vcSampleRate) ? 0 : pdBias;
        } else {
            count_QaPdRes[slotNum] = 0;
            count_pdRes[slotNum] += 1;
            pdBias = (count_pdRes[slotNum] == 120 * pvcHr11->vcSampleRate) ? pdBias - 1 : pdBias;
            pdResMax[slotNum] = (count_pdRes[slotNum] == 120 * pvcHr11->vcSampleRate) ? pdResMax[slotNum] + 1 : pdResMax[slotNum];
        }
    } else {
        count_QaPdRes[slotNum] = 0;
        count_pdRes[slotNum] = 0;
    }
    return ret;
}

/*
 * @brief
 * Function Before getsample value,prejude Status.
 *
 **/
static vcHr11Ret_t vcHr11ReadStatus(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    pvcHr11->vcFifoReadFlag = 0;
    uint8_t vcHr11SlotCnt = 0;
    if (pvcHr11->vcHr11Status & VCREG1_CONFLICT) {
        return VCHR11RET_ISCONFLICT;
    }
    if (pvcHr11->vcHr11Status & VCREG1_INSAMPLE) {
        return VCHR11RET_ISINSAMPLE;
    }
#if (VCWEAR_READ_FIFO_MODE == VCWEAR_READ_FIFO_BY_INT)	//中断的方式需要判断中断原因
    if (pvcHr11->regConfig[1] & 0x10) {
#if (VCWEAR_STATUS_DETECTION_MODE != VCWEAR_STATUS_DETECTION_OFF)
        if (pvcHr11->intReason & 0x10) {
            ret |= vcHr11WearStatusDetection(pvcHr11);
        }
#endif
    }
#else	//定时器的版本直接调用，因为可能PS中断原因被PPG采样清除掉
    ret |= vcHr11WearStatusDetection(pvcHr11);
#endif
    if ((ret == VCHR11RET_UNWEARTOISWEAR) && (pvcHr11->wearStatus == VCHR11WEARST_ISWEAR)) {
        // Set psflag to 1 when the state is switched (from unwear to iswear)
        // to prevent psflag and fifoflag from being 0 at the same time
        pvcHr11->vcPsFlag = 1;
    }
    if ((ret != VCHR11RET_UNWEARTOISWEAR) && (pvcHr11->wearStatus == VCHR11WEARST_ISWEAR)) {
        // Clear the psflag that appears after the state switch (from unwear to iswear)
        pvcHr11->vcPsFlag = 0;
    }
    for (vcHr11SlotCnt = 0; vcHr11SlotCnt < totalSlot; vcHr11SlotCnt++) {
        slotCount = (totalSlot == 2) ? vcHr11SlotCnt : (enSlot[0] ? 0 : 1);

        vcHr11AdjustPDResMax(pvcHr11, slotCount);

        ledCurrent[slotCount] = pvcHr11->regConfig[7 + slotCount] & 0x7f;
        ppgGain[slotCount] = pvcHr11->regConfig[7 + slotCount] & 0x80;
        pdRes[slotCount] = (pvcHr11->regConfig[10 + slotCount] & 0x70) >> 4;
        pdResSet[slotCount] = pvcHr11->regConfig[10 + slotCount] & 0x8F;

        if (pvcHr11->intReason & VCREG1_INTENV) {
            ret |= vcHr11EnvAdjust(pvcHr11, slotCount);
        }
        if (pvcHr11->intReason & VCREG1_INTOV) {
            ret |= vcHr11OvloadAdjust(pvcHr11, slotCount);
        }
        if (pvcHr11->intReason & VCREG1_INTPPG) {
            ret |= vcHr11PpgAdjust(pvcHr11, slotCount);
        } else {
            pvcHr11->adjustInfo[slotCount].directionLast = VCHR11ADJUSTDIRECTION_NULL;
            pvcHr11->adjustInfo[slotCount].direction = VCHR11ADJUSTDIRECTION_NULL;
        }
    }
    if (pvcHr11->regConfig[1] & 0x04) {
        if (pvcHr11->intReason & VCREG1_INTFIFO) {
#if (VCWEAR_STATUS_DETECTION_MODE != VCWEAR_STATUS_DETECTION_OFF)
#if (VCWEAR_READ_FIFO_MODE == VCWEAR_READ_FIFO_BY_INT)
            ret |= vcHr11WearStatusDetection(pvcHr11);
#endif
#endif
            pvcHr11->vcFifoReadFlag = 1;
        }
    }
#if VCWEAR_READ_FIFO_MODE == VCWEAR_READ_FIFO_BY_TIME
    pvcHr11->vcFifoReadFlag = 1;
#endif
    return ret;
}

/*
 * @brief
 * Function: Read Pre and EnvValue
 *
 **/
static vcHr11Ret_t vcHr11ReadReg(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    uint8_t vcRegisterValue[6] = {0};
    ret |= vcHr11ReadRegisters(VCREG1, vcRegisterValue, 6);

    pvcHr11->vcHr11Status = vcRegisterValue[0];
    pvcHr11->intReason = vcRegisterValue[1];

    pvcHr11->sampleData.envValue[0] = vcRegisterValue[3] >> 4;
    pvcHr11->sampleData.preValue[0] = vcRegisterValue[3] & 0x0F;
    pvcHr11->sampleData.envValue[1] = vcRegisterValue[4] >> 4;
    pvcHr11->sampleData.preValue[1] = vcRegisterValue[4] & 0x0F;
    pvcHr11->sampleData.envValue[2] = vcRegisterValue[5] >> 4;
    pvcHr11->sampleData.psValue = vcRegisterValue[5] & 0x0F;
    DEBUG_LOG("env0=%d,env1=%d,env2=%d", pvcHr11->sampleData.envValue[0], pvcHr11->sampleData.envValue[1], pvcHr11->sampleData.envValue[2]);
    DEBUG_LOG("pre0=%d,pre1=%d,psv2=%d", pvcHr11->sampleData.preValue[0], pvcHr11->sampleData.preValue[1], pvcHr11->sampleData.psValue);

    ret |= vcHr11ReadRegisters(VCREG17, vcRegisterValue, 6);
    pvcHr11->sampleData.pdResValue[0] = (vcRegisterValue[3] >> 4) & 0x07;
    pvcHr11->sampleData.currentValue[0] = vcRegisterValue[0] & 0x7F;
    pvcHr11->sampleData.pdResValue[1] = (vcRegisterValue[4] >> 4) & 0x07;
    pvcHr11->sampleData.currentValue[1] = vcRegisterValue[1] & 0x7F;
    pvcHr11->sampleData.pdResValue[2] = (vcRegisterValue[5] >> 4) & 0x07;
    pvcHr11->sampleData.currentValue[2] = vcRegisterValue[2] & 0x7F;

    return ret;
}

/*
 * @brief
 * Function: 定时器或者中断版本，只通过宏去控制，此后的版本均通过 vcHr11GetSampleValues 该函数获取数据
 **/
vcHr11Ret_t vcHr11GetSampleValues(vcHr11_t *pvcHr11, uint8_t *DataLen)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;
    int16_t secondCnt = pvcHr11->vcSampleRate * 180 / (fifoIntDiv + 1); // 3min
    // int16_t secondCnt = pvcHr11->vcSampleRate*10/(fifoIntDiv+1);//10s
    vcHr11ReadReg(pvcHr11);

    switch (pvcHr11->workMode) {
    case VCWORK_MODE_HRWORK:
        ret |= vcHr11ReadStatus(pvcHr11);
        if (!pvcHr11->oscCheckFinishFlag) {
            vcHr11CalculateOSCFreq(pvcHr11); // Consider the 20k Adjust to five mintures called.
        }
        if (pvcHr11->vcFifoReadFlag) {
            oscCnt += 1;
            if (oscCnt >= secondCnt) {
                vcHr11CalculateOSCFreq(pvcHr11); // Consider the 20k Adjust to five mintures called.
            }
            if (pvcHr11->oscCheckFinishFlag) {
                ret |= vcHr11ReadFifo(pvcHr11, DataLen);
            }
        }
        break;

    case VCWORK_MODE_SPO2WORK:
        ret |= vcHr11ReadStatus(pvcHr11);
        if (!pvcHr11->oscCheckFinishFlag) {
            vcHr11CalculateOSCFreq(pvcHr11);
        }
        if (pvcHr11->vcFifoReadFlag == 1) {
            oscCnt += 1;
            if (oscCnt >= secondCnt) {
                /* Consider the 20k Adjust to three mintures called */
                vcHr11CalculateOSCFreq(pvcHr11);
            }
            if (pvcHr11->oscCheckFinishFlag) {
                ret |= vcHr11ReadFifo(pvcHr11, DataLen);
            }
        }
        break;

    case VCWORK_MODE_CROSSTALKTEST:
        ledCurrent[0] = pvcHr11->regConfig[7] & 0x7f;
        if (pvcHr11->intReason & VCREG1_INTOV) {
            ret |= vcHr11OvloadAdjust(pvcHr11, 0);
        }
        pvcHr11->sampleData.maxLedCur = ledMaxCurrent[0];
        /* PASS??: */
        //			((pvcHr11->sampleData.maxLedCur >= 100) && (pvcHr11->sampleData.preValue[0] <= 2)
        //    /*Monitor ps Value*/
        //     read pvcHr11->sampleData.psValue
        break;

    case VCWORK_MODE_NOISEWORK:
        ret |= vcHr11ReadStatus(pvcHr11);
        if (pvcHr11->vcFifoReadFlag == 1) {
            ret |= vcHr11ReadFifo(pvcHr11, DataLen);
        }
        break;

    case VCWORK_MODE_LPDETECTION:
        if (!pvcHr11->oscCheckFinishFlag) {
            vcHr11CalculateOSCFreq(pvcHr11);
        }
        oscCnt += 1;
        if (oscCnt >= secondCnt) {
            /* Consider the 20k Adjust to five mintures called */
            vcHr11CalculateOSCFreq(pvcHr11);
        }
#if (VCWEAR_STATUS_DETECTION_MODE != VCWEAR_STATUS_DETECTION_OFF)
        ret |= vcHr11WearStatusDetection(pvcHr11);
#endif
        pvcHr11->vcPsFlag = 1;
        break;
    default:
        break;
    }
    return ret;
}

/*
 * @brief
 * Function: Soft reset to make all registor
 * Write 8'h5A to 8'h3B cause soft reset.
 **/
vcHr11Ret_t vcHr11SoftReset(vcHr11_t *pvcHr11)
{
    vcHr11Ret_t ret = VCHR11RET_ISOK;

    uint8_t Reset = 0x5A;
    ret |= vcHr11WriteRegisters(VCREG10, (uint8_t *)&Reset, VCREG10_SIZE);
    oscCnt = 0;
    pvcHr11->oscCheckFinishFlag = 0;
    fifoWriteIndex = 0;
    fifoReadIndex = 0x80;
    return ret;
}

#endif