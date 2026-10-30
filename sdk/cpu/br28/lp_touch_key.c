#include "includes.h"
#include "asm/power/p11.h"
#include "asm/power/p33.h"
#include "asm/lp_touch_key_api.h"
#include "asm/lp_touch_key_tool.h"
#include "device/key_driver.h"
#include "key_event_deal.h"
#include "app_config.h"
#include "asm/charge.h"



#define LOG_TAG_CONST       LP_KEY
#define LOG_TAG             "[LP_KEY]"
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"



#if TCFG_LP_TOUCH_KEY_ENABLE


static struct lp_touch_key_platform_data *__this = NULL;

static volatile u8 touch_irq_before_init = 0;


#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE

static u32 touch_bt_tool_enable = 0;
static int (*touch_bt_online_debug_init)(void) = NULL;
static int (*touch_bt_online_debug_send)(u8, u16) = NULL;
static int (*touch_bt_online_debug_key_event_handle)(u8, void *) = NULL;

void lp_touch_key_debug_init(u32 bt_debug_en, void *bt_debug_init, void *bt_debug_send, void *bt_debug_event)
{
    touch_bt_tool_enable = bt_debug_en;
    touch_bt_online_debug_init = bt_debug_init;
    touch_bt_online_debug_send = bt_debug_send;
    touch_bt_online_debug_key_event_handle = bt_debug_event;
}
#endif

void lp_touch_key_init(struct lp_touch_key_platform_data *config)
{
    log_info("%s >>>>", __func__);

    __this = config;
    if ((!__this) || (!__this->lpctmu_cfg)) {
        return;
    }

    M2P_CTMU_CMD = 0;
    M2P_CTMU_CH_DEBUG = 0;

    M2P_CTMU_SOFTOFF_WAKEUP_TIMEL   = (__this->softoff_wakeup_time   >> 0) & 0xff;
    M2P_CTMU_SOFTOFF_WAKEUP_TIMEH   = (__this->softoff_wakeup_time   >> 8) & 0xff;

    lpctmu_init(__this->lpctmu_cfg);

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE

    lp_touch_key_debug_init(
        1,
        lp_touch_key_online_debug_init,
        lp_touch_key_online_debug_send,
        lp_touch_key_online_debug_key_event_handle);

    if ((touch_bt_tool_enable) && (touch_bt_online_debug_init)) {
        touch_bt_online_debug_init();
    }
#endif

    if (touch_irq_before_init) {
        log_debug("lp touch key irq before init %d !", touch_irq_before_init);
        lp_touch_key_event_irq_handler();
        touch_irq_before_init = 0;
    }

    log_debug("M2P_CTMU_CH_ENABLE              = 0x%x\n", M2P_CTMU_CH_ENABLE);
    log_debug("M2P_CTMU_CH_WAKEUP_EN           = 0x%x\n", M2P_CTMU_CH_WAKEUP_EN);
    log_debug("M2P_CTMU_CH_DEBUG               = 0x%x\n", M2P_CTMU_CH_DEBUG);
    log_debug("M2P_CTMU_CH_CFG                 = 0x%x\n", M2P_CTMU_CH_CFG);
    log_debug("M2P_CTMU_SCAN_TIME              = %d\n", M2P_CTMU_SCAN_TIME);
    log_debug("M2P_CTMU_LOWPOER_SCAN_TIME      = %d\n", M2P_CTMU_LOWPOER_SCAN_TIME);
    log_debug("M2P_CTMU_SOFTOFF_WAKEUP_TIMEL   = %d\n", M2P_CTMU_SOFTOFF_WAKEUP_TIMEL);
    log_debug("M2P_CTMU_SOFTOFF_WAKEUP_TIMEH   = %d\n", M2P_CTMU_SOFTOFF_WAKEUP_TIMEH);
}


static void lp_touch_key_notify_key_event(struct sys_event *event, u32 ch)
{
    event->type = SYS_KEY_EVENT;
    event->u.key.type = KEY_DRIVER_TYPE_CTMU_TOUCH;     //区分按键类型
    event->arg  = (void *)DEVICE_EVENT_FROM_KEY;

#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
    if ((touch_bt_tool_enable) && (touch_bt_online_debug_key_event_handle)) {
        if (touch_bt_online_debug_key_event_handle(ch, (void *)event)) {
            return;
        }
    }
#endif

    sys_event_notify(event);
}

void lp_touch_key_event_irq_handler()
{
    if (!__this) {
        touch_irq_before_init += 1;
        return;
    }

    u8 ch, ctmu_event = P2M_CTMU_KEY_EVENT;
    P2M_CTMU_KEY_EVENT = 0;

    if (ctmu_event & CTMU_P2M_CHx_RES_EVENT) {
#if TCFG_LP_TOUCH_KEY_BT_TOOL_ENABLE
        ch = P2M_CTMU_RES_CH;
        u16 chx_res = (P2M_MESSAGE_ACCESS(P2M_MASSAGE_CTMU_CH0_H_RES + ch * 2) << 8) | P2M_MESSAGE_ACCESS(P2M_MASSAGE_CTMU_CH0_L_RES + ch * 2);
        if ((touch_bt_tool_enable) && (touch_bt_online_debug_send)) {
            touch_bt_online_debug_send(ch, chx_res);
        }
#endif
    } else if (ctmu_event & CTMU_P2M_CHx_FALLING_EVENT) {
        ch = P2M_CTMU_FALLING_CH;
        log_debug("CH%d: FALLING", ch);
    } else if (ctmu_event & CTMU_P2M_CHx_RAISING_EVENT) {
        ch = P2M_CTMU_RAISING_CH;
        log_debug("CH%d: RAISING", ch);
    }
}

u32 lp_touch_key_power_on_status()
{
    return 0;
}

void lp_touch_key_disable(void)
{
    log_debug("%s", __func__);
    if (!__this) {
        return;
    }
    lpctmu_disable();
}

void lp_touch_key_enable(void)
{
    log_debug("%s", __func__);
    if (!__this) {
        return;
    }
    lpctmu_enable();
}

void lp_touch_key_charge_mode_enter(void)
{
    log_debug("%s", __func__);
    if (!__this) {
        return;
    }
    if (__this->charge_mode_keep_touch) {
        return;
    }
    log_debug("lpctmu disable\n");
    lpctmu_disable();
}

void lp_touch_key_charge_mode_exit(void)
{
    log_debug("%s", __func__);
    if (!__this) {
        return;
    }
    if (__this->charge_mode_keep_touch) {
        return;
    }

    log_debug("lpctmu enable\n");
    lpctmu_enable();
}

u8 lp_touch_key_softoff_mode_query(void)
{
    return lpctmu_is_sf_keep();
}

void p33_ctmu_key_event_irq_handler(void)
{
    lp_touch_key_event_irq_handler();
}

#endif


