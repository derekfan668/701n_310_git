#include "includes.h"
#include "asm/power/p11.h"
#include "asm/power/p33.h"
#include "asm/lpctmu_hw.h"

#if 1

#define LOG_TAG_CONST       LP_KEY
#define LOG_TAG             "[LP_KEY]"
/* #define LOG_ERROR_ENABLE */
/* #define LOG_DEBUG_ENABLE */
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

#else

#define log_debug   printf

#endif


static const u8 ch_port[LPCTMU_CHANNEL_SIZE] = {
    IO_PORTB_00,
    IO_PORTB_01,
    IO_PORTB_02,
    IO_PORTB_04,
    IO_PORTB_05,
};


static struct lpctmu_platform_data *__this = NULL;

static u8 softoff_keep_work = 0;

extern u32 __get_lrc_hz();


void lpctmu_send_m2p_cmd(enum CTMU_M2P_CMD cmd)
{
    P2M_CTMU_CMD_ACK = 0;
    M2P_CTMU_CMD = cmd;
    P11_SYSTEM->M2P_INT_SET = BIT(M2P_CTMU_INDEX);
    while (!(P2M_CTMU_CMD_ACK));
}

void lpctmu_port_init(u32 ch)
{
    u32 port = ch_port[ch];
    gpio_set_direction(port, 1);
    gpio_set_die(port, 0);
    gpio_set_pull_up(port, 0);
    gpio_set_pull_down(port, 0);

    SFR(P11_PORT->PB_SEL, ch, 1, 1);
    SFR(P11_PORT->PB_DIR, ch, 1, 1);
    SFR(P11_PORT->PB_DIE, ch, 1, 0);
    SFR(P11_PORT->PB_PU,  ch, 1, 0);
    SFR(P11_PORT->PB_PD,  ch, 1, 0);
}

void lpctmu_set_ana_lv_level(u32 level)
{
    SFR(P11_LPCTM->ANA0, 4, 2, level);
}

u32 lpctmu_get_ana_lv_level(void)
{
    u32 level = (P11_LPCTM->ANA0 >> 4) & 0b11;
    return level;
}

void lpctmu_set_ana_hv_level(u32 level)
{
    SFR(P11_LPCTM->ANA0, 6, 2, level);
}

u32 lpctmu_get_ana_hv_level(void)
{
    u32 level = (P11_LPCTM->ANA0 >> 6) & 0b11;
    return level;
}

void lpctmu_set_ana_cur_level(u32 cur_level)
{
    SFR(P11_LPCTM->ANA0, 1, 3, cur_level);
}

u32 lpctmu_get_ana_cur_level(void)
{
    u32 level = (P11_LPCTM->ANA0 >> 1) & 0b111;
    return level;
}

void lpctmu_pcnt_init(void)
{
    u32 lrc_clk = __get_lrc_hz();
    u32 scan_time = M2P_CTMU_SCAN_TIME;
    u32 pcnt_prd = 0xffff - (scan_time * lrc_clk / 1000);

    P3_PCNT_CON = BIT(6);

    P3_PCNT_SET0 = pcnt_prd & 0xff;
    P3_PCNT_SET1 = pcnt_prd >> 8;
    //SET WE
    P3_PCNT_CON = BIT(4);
    //WKEN | LRC clk | EN
    P3_PCNT_CON = BIT(5) | BIT(3) | BIT(0);
}

void lpctmu_pcnt_disable(void)
{
    P3_PCNT_CON = BIT(6);
}

void lpctmu_pcnt_enable(void)
{
    lpctmu_pcnt_init();
}

u32 lpctmu_pcnt_is_working(void)
{
    if (P3_PCNT_CON & BIT(0)) {
        return 1;
    }
    return 0;
}

void lpctmu_dump(void)
{
    log_debug("P11_LPCTM->CON0 = 0x%x", P11_LPCTM->CON0);
    log_debug("P11_LPCTM->CON1 = 0x%x", P11_LPCTM->CON1);
    log_debug("P11_LPCTM->CON2 = 0x%x", P11_LPCTM->CON2);
    log_debug("P11_LPCTM->CON3 = 0x%x", P11_LPCTM->CON3);
    log_debug("P11_LPCTM->ANA0 = 0x%x", P11_LPCTM->ANA0);
    log_debug("P11_LPCTM->ANA1 = 0x%x", P11_LPCTM->ANA1);
    log_debug("P11_LPCTM->RES  =   %d", P11_LPCTM->RES);
}

void lpctmu_init(struct lpctmu_platform_data *pdata)
{
    __this = pdata;
    if (!__this) {
        return;
    }

    u8 ch_en = 0;
    u8 ch_wkp_en = 0;
    for (u32 ch = 0; ch < LPCTMU_CHANNEL_SIZE; ch ++) {
        if (__this->ch[ch].enable) {
            ch_en |= BIT(ch);
            lpctmu_port_init(ch);
            if (__this->ch[ch].wakeup_en) {
                ch_wkp_en |= BIT(ch);
            }
        }
    }
    softoff_keep_work = ch_wkp_en;

    M2P_CTMU_CH_ENABLE = ch_en;
    M2P_CTMU_CH_WAKEUP_EN = ch_wkp_en;
    M2P_CTMU_SCAN_TIME = __this->sample_scan_time;
    M2P_CTMU_LOWPOER_SCAN_TIME = __this->lowpower_sample_scan_time;

    if (!(is_wakeup_source(PWR_WK_REASON_P11))) {

        lpctmu_pcnt_disable();
        memset(P11_LPCTM, 0, sizeof(P11_LPCTM_TypeDef));

        lpctmu_send_m2p_cmd(REQUEST_LPCTMU_IRQ);

        //时钟源配置
        u32 lpctmu_clk = __get_lrc_hz();
        log_debug("lpctmu_clk = %dHz", lpctmu_clk);
        SFR(P11_LPCTM->CON0, 8, 1, 0);  //ctm_osc sel lrc200KHz
        SFR(P11_LPCTM->CON0, 3, 1, 0);  //ctm_clk sel ctm_osc
        //设置分频
        SFR(P11_LPCTM->CON2, 0, 4, 2); //div = 2^2 = 4分频
        //每个通道采集的周期，常设几个毫秒
        u16 det_prd = __this->sample_window_time * lpctmu_clk / 4 / 1000 - 1;
        SFR(P11_LPCTM->CON3, 0, 8, det_prd);

        SFR(P11_LPCTM->CON1, 0, 5, ch_en);

        lpctmu_set_ana_lv_level(__this->ana_lv_level);

        lpctmu_set_ana_hv_level(__this->ana_hv_level);

        lpctmu_set_ana_cur_level(__this->ana_cur_level);

        SFR(P11_LPCTM->CON0, 2, 1, 1);      //LPCTM WKUP en
        SFR(P11_LPCTM->CON0, 4, 1, 1);      //模拟滤波使能

        SFR(P11_LPCTM->ANA0, 0, 1, 0);
        SFR(P11_LPCTM->ANA1, 3, 1, 0);
        SFR(P11_LPCTM->ANA1, 4, 1, 0);

        SFR(P11_LPCTM->CON0, 6, 1, 1);     //clear pnd
        SFR(P11_LPCTM->CON0, 1, 1, 1);     //模块的RES中断使能
        SFR(P11_LPCTM->CON0, 0, 1, 0);     //模块先关闭

    }

    lpctmu_pcnt_init();

    lpctmu_dump();
}

void lpctmu_disable(void)
{
    lpctmu_pcnt_disable();
    SFR(P11_LPCTM->CON0, 0, 1, 0);      //模块总开关
}

void lpctmu_enable(void)
{
    SFR(P11_LPCTM->CON0, 0, 1, 1);      //模块总开关
    lpctmu_pcnt_enable();
}

u32 lpctmu_is_sf_keep(void)
{
    if (!__this) {
        return 0;
    }
    if (lpctmu_pcnt_is_working()) {
        if (softoff_keep_work) {
            return 1;
        } else {
            lpctmu_disable();
        }
    }
    return 0;
}


