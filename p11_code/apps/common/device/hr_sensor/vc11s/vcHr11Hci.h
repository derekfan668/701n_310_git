
#ifndef LIB_vcHr11_HCI_vcHr11HCI_H_
#define LIB_vcHr11_HCI_vcHr11HCI_H_

#include "stdint.h"
#include "app_config.h"
#include "includes.h"


#define Drive_Version  V_1_9_4

#define HW_HALVE_POWER_CAP 1 //sample config param halve & oversample double

/* HW_LC09A_PROJ(1), single green led
 * HW_LC09A_PROJ(0), 10A/11S project
 */
#define HW_LC09A_PROJ 1
/*****************************************************************************
*
*					         Register Address Declaration
*
******************************************************************************/
/* Read Only */
#define VCREG0					        				(0x00)
#define VCREG0_SIZE                                     (1)
#define VCREG1                                          (0x01)
#define VCREG1_SIZE                                     (1)
#define VCREG2                                          (0x02)
#define VCREG2_SIZE                                     (1)
#define VCREG3                                          (0x03)
#define VCREG3_SIZE                                     (1)
#define VCREG4                                          (0x04)
#define VCREG4_SIZE                                     (1)
#define VCREG5                                          (0x05)
#define VCREG5_SIZE                                     (1)
#define VCREG6                                          (0x06)
#define VCREG6_SIZE                                     (1)
#define VCREG7                                          (0x07)
#define VCREG7_SIZE                                     (2)
#define VCREG8                                          (0x09)
#define VCREG8_SIZE                                     (2)
#define VCREG9                                          (0x80)
#define VCREG9_SIZE                                     (128)

/* Write Only */
#define VCREG10                                         (0x3B)
#define VCREG10_SIZE                                    (1)

/* Read or Write */
#define VCREG11                                         (0x10)
#define VCREG11_SIZE                                    (1)
#define VCREG12                                         (0x11)
#define VCREG12_SIZE                                    (1)
#define VCREG13                                         (0x12)
#define VCREG13_SIZE			       					(1)
#define VCREG14                                         (0x13)
#define VCREG14_SIZE                                    (1)
#define VCREG15                                         (0x14)
#define VCREG15_SIZE                                    (2)
#define VCREG16                                         (0x16)
#define VCREG16_SIZE                                    (1)
#define VCREG17                                         (0x17)
#define VCREG17_SIZE                                    (1)
#define VCREG18                                         (0x18)
#define VCREG18_SIZE                                    (1)
#define VCREG19                                         (0x19)
#define VCREG19_SIZE                                    (1)
#define VCREG20                                         (0x1A)
#define VCREG20_SIZE                                    (1)
#define VCREG21                                         (0x1B)
#define VCREG21_SIZE                                    (1)
#define VCREG22                                         (0x1C)
#define VCREG22_SIZE                                    (1)
#define VCREG23                                         (0x1D)
#define VCREG23_SIZE                                    (1)
#define VCREG24                                         (0x1E)
#define VCREG24_SIZE                                    (1)
#define VCREG25                                         (0x1F)
#define VCREG25_SIZE                                    (1)
#define VCREG26                                         (0x20)
#define VCREG26_SIZE                                    (1)
#define VCREG27                                         (0x21)
#define VCREG27_SIZE                                    (1)
#define VCREG28                                         (0x22)
#define VCREG28_SIZE                                    (1)


/* Bit fields for VCREG1s */
#define VCREG1_CONFLICT                                 (0x01 << 4)
#define VCREG1_INSAMPLE		    		                (0x01 << 3)
#define VCREG1_INTPS			    		            (0x01 << 4)
#define VCREG1_INTOV			   			            (0x01 << 3)
#define VCREG1_INTFIFO	      		                    (0x01 << 2)
#define VCREG1_INTENV			    		            (0x01 << 1)
#define VCREG1_INTPPG			    		            (0x01 << 0)

/* Threshold Settings */
#define	VCENV_TH										 8  //Causes of ENV interruption
#define	VCPPG_TH										 10 //Causes of PPG interruption PPG_TH = 300
#define	VCPS_TH   								         4
#define VCADJUST_INCREASE						         22 // 1.4 << 4 = 22.4//1.4f
#define VCADJUST_DECREASE					             11 // 0.7 << 4 = 11.2//0.7f
#define	VCADJUST_STEP_MAX						         32
#define	VCADJUST_STEP_MIN						         1

#define VCUNWEAR_CNT						             3
#define VCISWEAR_CNT							         1

#define SLOTCHNU  										 3

#define VCWEAR_STATUS_DETECTION_OFF	                     0
#define VCWEAR_STATUS_DETECTION_ON	                     1
#define VCWEAR_STATUS_DETECTION_MODE VCWEAR_STATUS_DETECTION_ON

#define VCWEAR_READ_FIFO_BY_TIME	 (0)
#define VCWEAR_READ_FIFO_BY_INT	   (1)
#if TCFG_HR_SENSOR_READ_BY_INT
#define VCWEAR_READ_FIFO_MODE	VCWEAR_READ_FIFO_BY_INT
#else
#define VCWEAR_READ_FIFO_MODE	VCWEAR_READ_FIFO_BY_TIME
#endif

#define SportMotionEn                                    0
#define BiometricEn                                      0


/*****************************************************************************
*
*					           Data Type Declaration
*
******************************************************************************/
typedef enum {
    VCWORK_MODE_HRWORK        = 0,
    VCWORK_MODE_CROSSTALKTEST = 1,
    VCWORK_MODE_SPO2WORK      = 2,
    VCWORK_MODE_GSENSORWORK   = 3,
    VCWORK_MODE_TEMPWORK      = 4,
    VCWORK_MODE_NOISEWORK     = 5,
    VCWORK_MODE_LPDETECTION   = 6,
    VCWORK_MODE_POWER_OFF     = 255,
} vcHr11Mode_t;

typedef enum {
    VCHR11ADJUSTDIRECTION_NULL = 0,
    VCHR11ADJUSTDIRECTION_UP   = 1,
    VCHR11ADJUSTDIRECTION_DOWN = 2,
} vcHr11AdjustDirection;

typedef enum {
    VERSION_A 	= 0,
    VERSION_B 	= 1,
} vcHr11Version_t;

typedef struct {
    vcHr11AdjustDirection direction;
    vcHr11AdjustDirection directionLast;
    uint16_t step;
} vcHr11AdjustInfo_t;

typedef enum {
    VCHR11WEARST_UNWEAR	= 0,
    VCHR11WEARST_ISWEAR	= 1,
} vcHr11WearStatus_e;

typedef enum {
    VCHR11RET_ISOK                = 0,
    VCHR11RET_ISWEARTOUNWEAR      = 1,
    VCHR11RET_UNWEARTOISWEAR      = 2,
    VCHR11RET_ISERR               = 4,
    VCHR11RET_ISCONFLICT          = 8,
    VCHR11RET_ISINSAMPLE          = 16,
    VCHR11RET_SOLTISOVERLOAD      = 32,
    VCHR11RET_PSISOVERLOAD        = 64,
    VCHR11RET_ENVCURRENTISLARGE   = 128,
    VCHR11RET_PSENVCURRENTISLARGE = 256,
    VCHR11RET_PPGCANNOTADJUSTABLE = 512,
    VCHR11RET_FIFOISOVERFLOW      = 1024,
    VCHR11RET_PDRESISMAX          = 2048,
    VCHR11RET_PDRESCANNOTRELEASE  = 4096,
} vcHr11Ret_t;

typedef struct {
    uint8_t maxLedCur;
    uint8_t pdResValue[3];
    uint8_t currentValue[3];
    uint8_t psValue;	    //PS Sample value.
    uint8_t preValue[2];	//Environment Sample value.
    uint8_t envValue[3];	//Environment Sample value.
    uint16_t ppgValue[128];	//PPG sample value.
} vcHr11SampleData_t;

typedef struct {
    bool isRunning;
    bool vcFifoReadFlag;
    bool vcPsFlag;
    bool oscCheckFinishFlag;

    uint8_t regConfig[17];
    uint8_t unWearCnt;
    uint8_t isWearCnt;
    uint8_t vcHr11Status;
    uint8_t intReason;
    uint8_t devId;
    vcHr11Mode_t workMode;
    vcHr11Version_t version;
    vcHr11WearStatus_e wearStatus;

    uint16_t vcSampleRate;
    uint16_t mcuOscValue;
    uint16_t mcuSclRate;
    vcHr11AdjustInfo_t adjustInfo[2];
    vcHr11SampleData_t sampleData;

} vcHr11_t;



/*****************************************************************************
*					             Function Declaration
******************************************************************************/
/*
 * @brief
 *   Get sample datas from vcHr11.
 *
 **/
vcHr11Ret_t  vcHr11GetSampleValues(vcHr11_t *pvcHr11, uint8_t *DataLen);

/*
* @brief
*   Manual start sample of vcHr11.
*
**/
vcHr11Ret_t vcHr11StartSample(vcHr11_t *pvcHr11);

/*
* @brief
*   Manual stop sample of vcHr11.
*
**/
vcHr11Ret_t vcHr11StopSample(vcHr11_t *pvcHr11);

/*
* @brief
*   Manual SoftReset the vcHr11
*
**/
vcHr11Ret_t vcHr11SoftReset(vcHr11_t *pvcHr11);

/*
* @brief
*   init the vcHr11
*
**/
extern void vcHr11Init(vcHr11_t *pVcHr11, vcHr11Mode_t vcHr11WorkMode);

/*
 * @brief
 *
 **/
extern vcHr11Ret_t vcHr11ReadRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len);

/*
 * @brief
 *
 *
 **/
extern vcHr11Ret_t vcHr11WriteRegisters(uint8_t startAddress, uint8_t *pRegisters, uint8_t len);

/*
 * @brief
 *   Write Registers to vcHr11. This function needs to be implemented
 *   according to the MCU.
 **/
extern uint32_t vcHr11GetRtcCountFromMCU(void);

/*
 * @brief
 *
 *
 **/
extern void green_led_off_state_gsensor_abs_sum_diff_func(int32_t x_axis_value, int32_t y_axis_value, int32_t z_axis_value);

typedef vcHr11Ret_t (* vcHr11_read_cb)(uint8_t startAddress, uint8_t *pRegisters, uint8_t len);
typedef vcHr11Ret_t (* vcHr11_write_cb)(uint8_t startAddress, uint8_t *pRegisters, uint8_t len);


#define DEBUG_LOG(fmt,...)     //printf("[vchr] %s " fmt "\n",__func__, ##__VA_ARGS__)



#endif
