#if defined(TCFG_HRS3602_DEV_ENABLE) && TCFG_HRS3602_DEV_ENABLE
#ifndef _HRS3600_H_
#define _HRS3600_H_
#include <stdint.h>
#include <stdbool.h>

#define TIMER_READ_MODE

#define HRS_ALG_LIB

#define IR_CHECK_TOUCH

#define CHECK_TOUCH_DIFF

//#define TESTVEC

#define TYHX_DEMO

#define HRS_BLE_APP

#define RTT_DEBUG

#define GSENSER_DATA

#define NEW_GSEN_SCHME

#ifdef RTT_DEBUG
#define LOG(fmt,...)              printf("[3602] %s " fmt "\n",__func__, ##__VA_ARGS__)
#define  DEBUG_PRINTF(fmt,...)    //printf("[3602] %s " fmt "\n",__func__, ##__VA_ARGS__)
#else
#define	 DEBUG_PRINTF(...)
#endif

typedef struct {
    int16_t AXIS_X;
    int16_t AXIS_Y;
    int16_t AXIS_Z;
} GsensorRaw_t;


void Hrs3602_chip_disable(void);
void Hrs3602_chip_enable(void);

bool Hrs3602_chip_init(void);
void Hrs3602_chip_reset(void);
void heart_rate_meas_timeout_handler(void *p_context);
void Hrs3602_INT_init(void);
void Hrs3602_INT_enable(void);
void Hrs3602_INT_disable(void);

bool    Hrs3602_write_reg(uint8_t addr, uint8_t data);
uint8_t Hrs3602_read_reg(uint8_t addr);
bool Hrs3602_brust_read_reg(uint8_t addr, uint8_t *buf, uint8_t length);

void    Hrs3602_timeout_handler(void);
void    Hrs3602_init_touch_mode(void);
void    Hrs3602_Int_handle(void);
void    Hrs3602_blood_presure_Int_handle(void);
void    Hrs3602_chip_disable(void);
void    Hrs3602_alg_config(void);
extern void Hrs3602_set_first_hr_mode(uint8_t mode);
extern void Hrs3602_set_dc_thres_low(int32_t value);

void Hrs3602_bp_alg_config(void); // add ericy 20180625

bool Hrs3602_read_hrs(int32_t *hrm_data, int32_t *als_data);
bool Hrs3602_read_ps1(int32_t *infrared_data);
void Hrs3602_set_motion_status(uint16_t motion_status);

uint32_t hrs3602_timers_start(void);
uint32_t hrs3602_timers_stop(void);
void Hrs3300_button_handler(void);
void read_data_packet(int32_t *ps_data);

#endif // _HRS3600_H_
#endif

