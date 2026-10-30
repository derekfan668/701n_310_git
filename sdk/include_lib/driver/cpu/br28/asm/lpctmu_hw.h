#ifndef __LPCTMU_HW_H__
#define __LPCTMU_HW_H__


#include "typedef.h"



#define LPCTMU_CHANNEL_SIZE  5


enum CTMU_M2P_CMD {
    REQUEST_LPCTMU_IRQ = 0x50,
    REQUEST_LPTMR_IRQ,
    UPGRADE_ANA_CFG,
};

struct lpctmu_ch_cfg {
    u8 enable;
    u8 wakeup_en;
};

struct lpctmu_platform_data {
    u8 sample_window_time;              //采样窗口时间 ms
    u8 sample_scan_time;                //多久采样一次 ms
    u8 lowpower_sample_scan_time;       //软关机下多久采样一次 ms
    u8 ana_hv_level;                    //上限电压挡位
    u8 ana_lv_level;                    //下限电压挡位
    u8 ana_cur_level;                   //电流挡位
    struct lpctmu_ch_cfg ch[LPCTMU_CHANNEL_SIZE];
};

#define LPCTMU_PLATFORM_DATA_BEGIN(data) \
    static const struct lpctmu_platform_data data = {

#define LPCTMU_PLATFORM_DATA_END() \
    .sample_window_time = 2, \
    .sample_scan_time = 20, \
    .lowpower_sample_scan_time = 100, \
    .ana_hv_level = 3, \
    .ana_lv_level = 0, \
    .ana_cur_level = 7, \
};


void lpctmu_send_m2p_cmd(enum CTMU_M2P_CMD cmd);

void lpctmu_set_ana_lv_level(u32 level);

u32 lpctmu_get_ana_lv_level(void);

void lpctmu_set_ana_hv_level(u32 level);

u32 lpctmu_get_ana_hv_level(void);

void lpctmu_set_ana_cur_level(u32 cur_level);

u32 lpctmu_get_ana_cur_level(void);

void lpctmu_init(struct lpctmu_platform_data *pdata);

void lpctmu_disable(void);

void lpctmu_enable(void);

u32 lpctmu_is_sf_keep(void);



#endif

