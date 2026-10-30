#ifndef _LP_TOUCH_KEY_API_
#define _LP_TOUCH_KEY_API_


#include "typedef.h"
#include "asm/lpctmu_hw.h"


enum CTMU_P2M_EVENT {
    CTMU_P2M_CHx_RES_EVENT = BIT(0),
    CTMU_P2M_CHx_FALLING_EVENT = BIT(1),
    CTMU_P2M_CHx_RAISING_EVENT = BIT(2),
};

struct lp_touch_key_platform_data {
    u16 charge_mode_keep_touch;
    u16 softoff_wakeup_time;
    struct lpctmu_platform_data *lpctmu_cfg;
};

void lp_touch_key_event_irq_handler();

u32 lp_touch_key_power_on_status();

void lp_touch_key_init(struct lp_touch_key_platform_data *config);

void lp_touch_key_disable(void);

void lp_touch_key_enable(void);

void lp_touch_key_charge_mode_enter();

void lp_touch_key_charge_mode_exit();



#endif

