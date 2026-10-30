#include "includes.h"
#include "asm/includes.h"
#include "asm/gpio.h"
#include "asm/dac.h"
#include "media/includes.h"
#include "pdm_link.h"
#include "audio_adc.h"
#include "sound/sound.h"
#include "board_config.h"

#define PLNK_DEBUG_ENABLE
#ifdef PLNK_DEBUG_ENABLE
//#define PLNK_LOG(fmt, ...) 	printf("[PLNK] "fmt, ##__VA_ARGS__)
#define PLNK_LOG 		y_printf
#else
#define PLNK_LOG(...)
#endif


static struct sound_pcm_stream *dac = NULL;
#define PLNK_TEST_ENABLE	1


static audio_plnk_t *pdm_hdl = NULL;

/*
 * Audio ADC 集成: 当 ch_num > 2 时，PLNK DMA 只处理前 2 路 CH0/CH1，
 * 额外的 PDM 通道通过 audio_adc DMIC 接口采集。
 *
 * DAT0 两个沿 → DMIC0 (rising) + DMIC1 (falling)
 * DAT1 两个沿 → DMIC2 (rising) + DMIC3 (falling)
 *
 * PLNK CH0 和 CH1 各占用一条数据线的一个沿，
 * 同一数据线的另一个沿通过 audio_adc 的 MIC_CTL1 路由到 ADC 数字通路。
 */
extern struct audio_adc_hdl adc_hdl;
static struct adc_mic_ch plnk_adc_mic_ch;
static struct audio_adc_output_hdl plnk_adc_output;
static s16 *plnk_adc_buf = NULL;
static u8 plnk_adc_opened = 0;

/* audio_adc 输出回调: 接收额外 DMIC 通道的数据 */
static void audio_plnk_adc_output(void *priv, s16 *data, int len)
{
#if 0
    /* data 格式: [dmicX_sample0, dmicY_sample0, dmicX_sample1, dmicY_sample1, ...] */
    s16 *data_s16 = (s16 *)data;
    u32 tlen = len * 2;
    int rlen = sound_pcm_write(dac, data_s16, tlen);
    if (rlen != tlen) {
        printf("ERR:%d,%d,%d\n", rlen, tlen, JL_AUDIO->ADC_PNS);
    }
#endif
}

/******************************************************
 * 双PDM MIC 双缓冲DMA数据布局（参考audio_pdm.c）：
 * --------------ch0-----------------ch1-----------------
 *                        |
 * -----buf0------buf1----------buf0-------buf1----------
 *
 * 每个buffer段大小为 buf_len 个 s16 采样点
 * CH0 buf0: buf + 0
 * CH0 buf1: buf + buf_len
 * CH1 buf0: buf + buf_len * 2
 * CH1 buf1: buf + buf_len * 3
 ******************************************************/
___interrupt
static void audio_plnk_isr(void)
{
    u8 buf_flag;
    s16 *buf = pdm_hdl->buf;
    u16 len = pdm_hdl->buf_len;
    s16 *data_buf;
    s16 *ch0_base;
    s16 *ch1_base;
    /* PLNK DMA 最多处理 2 路 (CH0/CH1)，>2 时额外通道由 audio_adc 处理 */
    u8 dma_ch = pdm_hdl->ch_num > 2 ? 2 : pdm_hdl->ch_num;
    if (JL_PLNK->CON & BIT(15)) {
        PLNK_PND_CLR();
        if (JL_PLNK->CON & BIT(9)) {
            buf_flag = 0;    //buf0 available
        } else {
            buf_flag = 1;    //buf1 available
        }
        if (dma_ch == 1) {
            /* 单通道模式：直接使用当前buffer */
            data_buf = (s16 *)buf;
            data_buf += buf_flag * len;
            if (pdm_hdl && pdm_hdl->output) {
                pdm_hdl->output(data_buf, len);
            }
        } else {
            /* 双通道模式：参考audio_pdm.c，将CH0/CH1数据交织到stereo_buf */
            if (buf_flag == 0) {
                /* buf0可用：读CH0 buf0 + CH1 buf0 */
                ch0_base = buf;
                ch1_base = buf + len * 2;
            } else {
                /* buf1可用：读CH0 buf1 + CH1 buf1 */
                ch0_base = buf + len;
                ch1_base = buf + len * 2 + len;
            }
            s16 *dst = (s16 *)pdm_hdl->stereo_buf;
            for (int i = 0; i < len; i++) {
                dst[0] = ch0_base[i];
                dst[1] = ch1_base[i];
                dst += 2;
            }
            if (pdm_hdl && pdm_hdl->output) {
                pdm_hdl->output(pdm_hdl->stereo_buf, len * 2);
            }
        }
    }
}

static void plnk_info_dump()
{
    printf("PLNK_CON = 0x%x\n", JL_PLNK->CON);
    printf("PLNK_CON1 = 0x%x\n", JL_PLNK->CON1);
    printf("PLNK_LEN = 0x%x\n", JL_PLNK->LEN);
    printf("PLNK_ADR = 0x%x\n", JL_PLNK->ADR);
    printf("PLNK_DOR = 0x%x\n", JL_PLNK->DOR);
    printf("ASS_CLK_CON = 0x%x\n", JL_ASS->CLK_CON);

#if 0
    printf("PLNK_CON = 0x%x", JL_PLNK->CON);
    printf("CLK_CON = 0x%x", JL_CLOCK->CLK_CON0);
    printf("PLNK_CON1 = 0x%x", JL_PLNK->CON1);
    printf("PLNK_LEN = 0x%x", JL_PLNK->LEN);
    printf("PLNK_ADR = 0x%x", JL_PLNK->ADR);
    printf("PLNK_SMR = %d", JL_PLNK->SMR);
    printf("PLNK_DOR = %d", JL_PLNK->DOR);
#endif
}

static void audio_plnk_sr_set(u16 sr)
{
    SFR(JL_ASS->CLK_CON, 1, 1, 0);
    JL_PLNK->DOR = pdm_hdl->plink_sclk / sr - 1;
    u8 div = PLNK_CLK / pdm_hdl->plink_sclk - 1;
    SFR(JL_ASS->CLK_CON, 2, 5, div);
}

static void audio_plnk_sclk_io_init(u8 port)
{
    r_printf("port === %d\n", port);
    gpio_set_fun_output_port(port, FO_PLNK_SCLK, 0, 1, LOW_POWER_FREE);
    gpio_set_direction(port, 0);
    gpio_set_die(port, 0);
    gpio_direction_output(port, 1);
}


static void audio_plnk_ch_io_init(u8 ch_index, u8 port)
{
    gpio_set_pull_down(port, 0);
    gpio_set_pull_up(port, 1);
    gpio_set_direction(port, 1);
    gpio_set_die(port, 1);
    if (ch_index) {
        gpio_set_fun_input_port(port, PFI_PLNK_DAT1, LOW_POWER_FREE);
    } else {
        gpio_set_fun_input_port(port, PFI_PLNK_DAT0, LOW_POWER_FREE);
    }
}

extern double pow(double x, double y);	/* wrapper pow */
static int log2(int x)
{
    u8 cnt = 0;
    x >>= 1;
    while (x) {
        x >>= 1;
        cnt++;
    }
    return cnt;
}

/*
 * 双PDM MIC的两种硬件接法：
 *
 * [接法1] 两根数据线（独立DAT0/DAT1）：
 *   MIC0 DAT -> IO_A (ch0_io)     MIC1 DAT -> IO_B (ch1_io)
 *   两个MIC都在SCLK同一边沿输出，分别走各自的data线
 *   CH0 mode: DATA0_SCLK_RISING_EDGE
 *   CH1 mode: DATA1_SCLK_RISING_EDGE
 *
 * [接法2] 一根数据线（multiplexed，共用DAT0）：
 *   MIC0 DAT ─┐
 *             ├── IO_A (ch0_io = ch1_io)
 *   MIC1 DAT ─┘
 *   MIC0在SCLK上升沿输出，MIC1在下降沿输出，共用DAT0线
 *   CH0 mode: DATA0_SCLK_RISING_EDGE
 *   CH1 mode: DATA0_SCLK_FALLING_EDGE  <-- 注意是DATA0的下降沿
 */

int audio_plnk_open(audio_plnk_t *hdl)
{
    if (hdl == NULL) {
        return -EINVAL;
    }

    pdm_hdl = hdl;

    if (pdm_hdl->sr % 8000) {
        pdm_hdl->plink_sclk = 3000000;
    } else {
        /*8000的倍数*/
        pdm_hdl->plink_sclk = 2400000;
    }

    PLNK_LOG("audio_plnk_open,ch=%d,sr=%d, sclk=%d", pdm_hdl->ch_num, pdm_hdl->sr, pdm_hdl->plink_sclk);

    PLNK_CON_RESET();

    /*
     * 双通道DMA buffer分配（参考audio_pdm.c）：
     * buf_len * ch_num * 2(ping-pong) * 2(s16字节数)
     * 即 buf_len 点 × 通道数 × 双缓冲 × sizeof(s16)
     */
    if (!pdm_hdl->buf) {
        pdm_hdl->buf = (s16 *)zalloc(pdm_hdl->buf_len * pdm_hdl->ch_num * 2 * sizeof(s16));
        ASSERT(pdm_hdl->buf);
    }
    JL_PLNK->ADR = (u32)pdm_hdl->buf;
    JL_PLNK->LEN = pdm_hdl->buf_len;
    /*
     * 双通道stereo buffer分配（参考audio_pdm.c）：
     * 用于ISR中将CH0/CH1数据交织，大小为 buf_len * ch_num 个s16
     */
    if (pdm_hdl->ch_num > 1) {
        if (!pdm_hdl->stereo_buf) {
            pdm_hdl->stereo_buf = zalloc(pdm_hdl->buf_len * pdm_hdl->ch_num * sizeof(s16));
            ASSERT(pdm_hdl->stereo_buf);
        }
    }
    /*
     *PLNK_CLK -> SMR -> SCLK -> DOR ->SR
     *1MHz <= SCLK <= 4MHz
     *sclk = PLINK_CLK / (SMR + 1)
     *5 <= SMR <= 47
     *PLNK_DOR， SR = SCLK / (PLNK_DOR + 1)
     *Version(A/B):0~4096
     *Version(C):1~2048
     */
    SFR(JL_PLNK->CON, 1, 1, 0);	// 通道0的数据及时钟来源（0:数字MIC  1:模拟MIC）
    SFR(JL_PLNK->CON, 5, 1, 0);	// 通道1的数据及时钟来源（0:数字MIC  1:模拟MIC）
    audio_plnk_sr_set(pdm_hdl->sr);
    audio_plnk_sclk_io_init(pdm_hdl->sclk_io);
    audio_plnk_ch_io_init(0, pdm_hdl->ch0_io);

    /* CH1 data IO init —— 双通道时使能第二路PDM MIC数据引脚 */
    if (pdm_hdl->ch_num > 1) {
        if (pdm_hdl->ch1_io == pdm_hdl->ch0_io) {
            /*
             * 共用同一IO（multiplexed模式）：
             * 引脚已由CH0初始化为PFI_PLNK_DAT0，无需再次配置
             * CH0/CH1通过不同SCLK边沿区分数据，由CH_MD寄存器控制
             */

            PLNK_LOG("CH0/CH1 share same IO: %d (multiplexed mode)", pdm_hdl->ch0_io);
        } else {
            /* 独立IO模式：CH1使用独立的DAT1引脚 */
            audio_plnk_ch_io_init(1, pdm_hdl->ch1_io);
        }
    }
    int dor = pdm_hdl->plink_sclk / pdm_hdl->sr;//PLNK_DOR 抽取因子,0~4095
    u8 cic_m = 1;//差分延迟因子,选择1/2
    u8 order = 3;//CIC级数,选择1/2/3
    int cic_gain = (int)pow((double)(dor * cic_m), (double)order);//CIC固有增益
    int cmp_gain = 4096;//pow(2.f, 12.f);
    printf("cic_gain %d, cmp_gain %d", cic_gain, cmp_gain);

    /*抽取滤波器差分延迟因子选择*/
    if (cic_m == 1) {
        SFR(JL_PLNK->CON1, 26, 1, 0);	//DFDLY1_SEL(0:M=1 1:M=2)
        SFR(JL_PLNK->CON1, 10, 1, 0); 	//DFDLY0_SEL(0:M=1 1:M=2)
    } else {
        SFR(JL_PLNK->CON1, 26, 1, 1);	//DFDLY1_SEL(0:M=1 1:M=2)
        SFR(JL_PLNK->CON1, 10, 1, 1); 	//DFDLY0_SEL(0:M=1 1:M=2)
    }

    /*抽取滤波器的阶数（级数）选择*/
    if (order == 3) {
        SFR(JL_PLNK->CON1, 24, 2, 0);	//ORDER1_SEL
        SFR(JL_PLNK->CON1, 8, 2, 0);	//ORDER0_SEL
    } else if (order == 2) {
        SFR(JL_PLNK->CON1, 24, 2, 1);	//ORDER1_SEL
        SFR(JL_PLNK->CON1, 8, 2, 1);	//ORDER0_SEL
    } else {
        SFR(JL_PLNK->CON1, 24, 2, 2);	//ORDER1_SEL
        SFR(JL_PLNK->CON1, 8, 2, 2);	//ORDER0_SEL
    }

    u8 shift_sel, scale_sel;
    if (cic_gain > cmp_gain) {
        SFR(JL_PLNK->CON1, 27, 1, 0);	//SHDIR1_SEL, 右移
        SFR(JL_PLNK->CON1, 11, 1, 0);	//SHDIR0_SEL, 右移
        shift_sel = (int)(log2(cic_gain) + 1) - 12;//+1操作是向上取整运算
        scale_sel = (32767 / (7 * cic_gain / pow(2.f, shift_sel)) - 1) / 0.125f;
    } else {
        SFR(JL_PLNK->CON1, 27, 1, 1);	//SHDIR1_SEL，左移
        SFR(JL_PLNK->CON1, 11, 1, 1);	//SHDIR0_SEL, 右移
        shift_sel = 12 - (int)(log2(cic_gain) + 1);//+1操作是向上取整运算
        scale_sel = (32767 / (7 * cic_gain * pow(2.f, shift_sel)) - 1) / 0.125f;
    }

    printf("shift_sel %d, scale_sel %d", shift_sel, scale_sel);

    SFR(JL_PLNK->CON1, 19, 5, shift_sel);	//SHIFT1_SEL[23:19]
    SFR(JL_PLNK->CON1, 3, 5, shift_sel); 	//SHIFT0_SEL[7:3]

    SFR(JL_PLNK->CON1, 16, 3, scale_sel);	//SCALE1_SEL
    SFR(JL_PLNK->CON1, 0, 3, scale_sel);	//SCALE0_SEL

    SFR(JL_PLNK->CON1, 28, 2, 0);
    SFR(JL_PLNK->CON1, 12, 2, 0);

    SFR(JL_PLNK->CON1, 30, 1, 0);	//CIC1_EN
    SFR(JL_PLNK->CON1, 14, 1, 0);	//CIC0_EN

    /*
    * 通道模式配置
    *
    * 当 ch_num > 2 时：
    *   PLNK CH0 和 CH1 各取一条数据线的一个沿（DMA 通路），
    *   同一数据线的另一个沿通过 audio_adc DMIC 采集（ADC 数字通路）。
    *
    * DMIC 映射（基于 MIC_CTL1 sel 值 4/5/6/7 = dmic0/1/2/3）：
    *   dmic0=4 (DAT0_rising)    dmic1=5 (DAT0_falling)
    *   dmic2=6 (DAT1_rising)    dmic3=7 (DAT1_falling)
    *
    * PLNK CH0 占用 DAT0 的一个沿 → 对侧沿用 AUDIO_ADC_DMIC_x
    * PLNK CH1 占用 DAT1 的一个沿 → 对侧沿用 AUDIO_ADC_DMIC_x
    */
    if (pdm_hdl->ch_num == 1) {
        PLNK_CH0_MD_SET(pdm_hdl->ch0_mode);
    } else {
        PLNK_CH0_MD_SET(pdm_hdl->ch0_mode);
        PLNK_CH1_MD_SET(pdm_hdl->ch1_mode);
    }

    /* >2 路 PDM：额外通道通过 audio_adc DMIC 采集 */
    if (pdm_hdl->ch_num > 2 && !plnk_adc_opened) { //如果分配了两个io ,默认  io 0采ch0, io1 采ch1
        /* CH0 占用了 DAT0 的某一个沿，对侧沿分配给 DMIC */
        u8 ch0_is_dat0_rising  = (pdm_hdl->ch0_mode == CH0MD_CH0_SCLK_RISING_EDGE);
        /* CH1 优先级: 如果 CH1 走 DAT0，和 CH0 共用 DAT0（multiplexed 模式）*/
        u8 ch1_on_dat1         = (pdm_hdl->ch1_io != pdm_hdl->ch0_io);
        /* DAT0 上空闲的沿：CH0 占 rising → 空闲 = falling (DMIC1)，反之亦然 */
        u16 extra_dmic0 = ch0_is_dat0_rising ? AUDIO_ADC_DMIC_0 : AUDIO_ADC_DMIC_1; //DMIC_0 是 io0 的上升沿
        PLNK_LOG("PDM MIC:%x,%d,%d\n", extra_dmic0, extra_dmic0, ch0_is_dat0_rising);
        u16 extra_dmic1;
        PLNK_LOG("plnk extra dmic: ch0_on_dat0, ch0_rising=%d, ch1_on_dat1=%d",
                 ch0_is_dat0_rising, ch1_on_dat1);
        audio_adc_mic_open(&plnk_adc_mic_ch, extra_dmic0, &adc_hdl);
        PLNK_LOG("plnk adc dmic0 opened: BIT(%d)", extra_dmic0 == AUDIO_ADC_DMIC_0 ? 8 : 9);
        if (ch1_on_dat1) {
            /* CH1 走独立 DAT1 → 空闲沿分配给 audio_adc */
            /* CH1 CH1MD 的值: 0=DAT1_rising, 1=DAT1_falling, 2/3=DAT0 */
            u8 ch1_is_dat1_rising = (pdm_hdl->ch1_mode == CH1MD_CH1_SCLK_RISING_EDGE);
            extra_dmic1 = ch1_is_dat1_rising ? AUDIO_ADC_DMIC_2 : AUDIO_ADC_DMIC_3; //DMIC_2是io1 的上升沿
            PLNK_LOG("PDM MIC1:%x,%d,%d\n", extra_dmic1, extra_dmic1, ch1_is_dat1_rising);
            audio_adc_mic_open(&plnk_adc_mic_ch, extra_dmic1, &adc_hdl);
            PLNK_LOG("plnk adc dmic1 opened on DAT1");
        }
        /* else: CH1 和 CH0 共用 DAT0，DAT1 上的两个沿都可以给 audio_adc */
        audio_adc_mic_set_sample_rate(&plnk_adc_mic_ch, pdm_hdl->sr);
        plnk_adc_buf = (s16 *)zalloc(pdm_hdl->buf_len * 2 * 2 * sizeof(s16));
        ASSERT(plnk_adc_buf);
        audio_adc_mic_set_buffs(&plnk_adc_mic_ch, plnk_adc_buf, pdm_hdl->buf_len * 2, 2);
        plnk_adc_output.handler = audio_plnk_adc_output;
        audio_adc_add_output_handler(&adc_hdl, &plnk_adc_output);
        plnk_adc_opened = 1;
    }
    PLNK_PND_CLR();
    request_irq(IRQ_PDM_LINK_IDX, 3, audio_plnk_isr, 0);

    return 0;
}

int audio_plnk_start(audio_plnk_t *hdl)
{
    JL_PLNK->CON |= BIT(8); //IE
    if (hdl->ch_num == 1) {
        SFR(JL_PLNK->CON, 0, 1, 1);  //CH0EN
        SFR(JL_PLNK->CON, 4, 1, 0);  //CH1 DIS
    } else {
        SFR(JL_PLNK->CON, 0, 1, 1);  //CH0EN
        SFR(JL_PLNK->CON, 4, 1, 1);  //CH1EN
    }
    plnk_info_dump();
    if (plnk_adc_opened) {
        audio_adc_mic_start(&plnk_adc_mic_ch);
    }
    return 0;
}

int audio_plnk_close(void)
{
    JL_PLNK->CON = 0;
    if (plnk_adc_opened) {
        audio_adc_del_output_handler(&adc_hdl, &plnk_adc_output);
        audio_adc_mic_close(&plnk_adc_mic_ch);
        if (plnk_adc_buf) {
            free(plnk_adc_buf);
            plnk_adc_buf = NULL;
        }
        plnk_adc_opened = 0;
    }
    if (pdm_hdl) {
        if (pdm_hdl->buf) {
            free(pdm_hdl->buf);
            pdm_hdl->buf = NULL;
        }
        if (pdm_hdl->stereo_buf) {
            free(pdm_hdl->stereo_buf);
            pdm_hdl->stereo_buf = NULL;
        }
    }
    pdm_hdl = NULL;
    return 0;
}

/* ================================================================
 * audio_adc 风格输出回调适配接口
 * ----------------------------------------------------------------
 * PLNK 的 output 回调:  void (*output)(void *buf, u16 len)
 *     len 单位是“采样点数”(s16 个数)，单通道 = buf_len，双通道 = buf_len*2
 * audio_enc 的 handler: void (*handler)(void *priv, s16 *data, int len)
 *     len 单位是“字节数”(点数 * 2)
 * 两者差异：
 *   1) 参数个数：2 个 vs 3 个（无 priv，这里传 NULL）
 *   2) len 单位：点数 vs 字节数（相差 2 倍）
 * ================================================================ */
static audio_plnk_t plnk_handle_hdl;
static void (*plnk_handle_output)(void *priv, s16 *data, int len);

static void audio_plnk_handle_bridge(void *buf, u16 len)
{
    if (plnk_handle_output) {
        plnk_handle_output(NULL, (s16 *)buf, (int)len * 2);
    }
}

int audio_plnk_open_handle(void (*output)(void *priv, s16 *data, int len), u16 sample_rate)
{
    plnk_handle_output = output;

    /* 引脚/接法参考下方 test_plink（DAT0: MIC0 + MIC2, DAT1: MIC1 + MIC3） */
    plnk_handle_hdl.sclk_io  = IO_PORTA_05;
    plnk_handle_hdl.ch_num    = 1;	/* esco 默认单 mic */
    plnk_handle_hdl.ch0_io    = IO_PORTA_12;                          // DAT0
    plnk_handle_hdl.ch1_io    = -1;                          // DAT1
    plnk_handle_hdl.ch0_mode  = CH0MD_CH0_SCLK_RISING_EDGE;
    plnk_handle_hdl.ch1_mode  = CH1MD_CH1_SCLK_RISING_EDGE;
    plnk_handle_hdl.sr        = sample_rate;
    plnk_handle_hdl.buf_len   = 256;
    plnk_handle_hdl.output    = audio_plnk_handle_bridge;

    int err = audio_plnk_open(&plnk_handle_hdl);
    if (err) {
        return err;
    }
    err = audio_plnk_start(&plnk_handle_hdl);
    if (err) {
        return err;
    }
    SFR(JL_ASS->CLK_CON, 0, 2, 3);	// 打开 PLNK 时钟（原 open_handle 里就有，7a3df2e19 移到了 test 里）
    return 0;
}

void audio_plnk_close_handle(void)
{
    audio_plnk_close();
}

/* DAC 回放初始化，供测试 demo 共用 */
static void sound_dac_power_setup(struct sound_pcm_stream *dac)
{
    sound_pcm_ctl_ioctl(dac, SNDCTL_IOCTL_POWER_ON, NULL);
    struct sound_volume volume = {
        .chmap = SOUND_CHMAP_FL | SOUND_CHMAP_FR,
        .volume = {10, 10},
    };
    sound_pcm_ctl_ioctl(dac, SNDCTL_IOCTL_SET_ANA_GAIN, &volume);
    volume.volume[0] = 16384;
    volume.volume[1] = 16384;
    sound_pcm_ctl_ioctl(dac, SNDCTL_IOCTL_SET_DIG_GAIN, &volume);
}

#if PLNK_TEST_ENABLE


static short const sin0_16k[16] = {
    0x0000, 0x30fd, 0x5a83, 0x7641, 0x7fff, 0x7642, 0x5a82, 0x30fc, 0x0000, 0xcf04, 0xa57d, 0x89be, 0x8000, 0x89be, 0xa57e, 0xcf05,
};

static u16 tx1_s_cnt = 0;
static int get_sine0_data(u16 *s_cnt, s16 *data, u16 points, u8 ch)
{
    while (points--) {
        if (*s_cnt >= 16) {
            *s_cnt = 0;
        }
        *data++ = sin0_16k[*s_cnt];
        if (ch == 2) {
            *data++ = sin0_16k[*s_cnt];
        }
        (*s_cnt)++;
    }
    return 0;
}

static void handle_output(void *buf, u16 len)
{
    s16 *paddr = (s16 *)buf;
    u32 len0 = len * 2; //len 是点数,乘以2为字节数
    u32 rlen;
    while (len0) {
        rlen = sound_pcm_write(dac, paddr, len0);
        if (rlen != len0) {
            printf("rlen = %d, len0 = %d\n", rlen, len0);
        }
        len0 -= rlen;
        paddr += rlen / 2;
        if (rlen == 0) {
        }
    }
}

/* 双PDM MIC测试配置示例 */

/* [接法1] 两根数据线模式 */
// audio_plnk_t	test_plink = {
//     .sclk_io   = IO_PORTA_01,
//     .ch_num	   = 2,
//     .ch0_io    = IO_PORTA_02,
//     .ch1_io    = IO_PORTA_03,                       // 独立的DAT1引脚
//     .ch0_mode  = CH0MD_CH0_SCLK_RISING_EDGE,
//     .ch1_mode  = CH1MD_CH1_SCLK_RISING_EDGE,
//     .sr        = 48000,
//     .buf_len   = 256,
// };

/* [接法3] 两根数据线，各挂2个MIC，共4MIC
 * DAT0: MIC0(rising 经 PLNK CH0) + MIC2(falling 经 audio_adc DMIC)
 * DAT1: MIC1(rising 经 PLNK CH1) + MIC3(falling 经 audio_adc DMIC)
 */
audio_plnk_t	test_plink = {
    .sclk_io   = IO_PORTA_01,
    .ch_num	   = 1,
    .ch0_io    = IO_PORTA_12,                         // DAT0: MIC0 + MIC2
    .ch1_io    = IO_PORTA_13,                         // DAT1: MIC1 + MIC3
    .ch0_mode  = CH0MD_CH0_SCLK_RISING_EDGE,          // MIC0 via PLNK CH0
    .ch1_mode  = CH1MD_CH1_SCLK_RISING_EDGE,          // MIC1 via PLNK CH1
    .sr        = 16000,
    .buf_len   = 256,
};


void audio_plnk_test(void)
{
    y_printf("pdm link dual mic demo\n");

    test_plink.output = handle_output;
    /*
     * 双通道buffer分配（参考audio_pdm.c）：
     * buf_len * ch_num * 2(ping-pong) * sizeof(s16)
     * 256 * 2 * 2 * 2 = 2048 bytes
     * buffer布局：[CH0_buf0][CH0_buf1][CH1_buf0][CH1_buf1]
     */
    test_plink.buf = (s16 *)zalloc(test_plink.buf_len * test_plink.ch_num * 2 * sizeof(s16));
    ASSERT(test_plink.buf);

    /* 双通道stereo buffer */
    test_plink.stereo_buf = zalloc(test_plink.buf_len * test_plink.ch_num * sizeof(s16));
    ASSERT(test_plink.stereo_buf);

    audio_plnk_open(&test_plink);
    audio_plnk_start(&test_plink);

    SFR(JL_ASS->CLK_CON, 0, 2, 3);
#if 0
    extern void app_audio_state_switch(u8 state, s16 max_volume);
    app_audio_state_switch(1, 16);
    audio_dac_set_volume(&dac_hdl, 16);
    audio_dac_set_sample_rate(&dac_hdl, test_plink.sr);
    audio_dac_start(&dac_hdl);
    /*打开PLINK_CLK*/
#endif

    int err = sound_pcm_create(&dac, "dac", 0);
    if (err) {
        printf("Create sound pcm error.\n");
        return;
    }
    sound_pcm_ctl_ioctl(dac, SNDCTL_IOCTL_SET_BIAS_TRIM, 0);
    sound_pcm_set_irq_handler(dac, NULL, NULL);//sound_dac_wakeup_irq_handler);
    sound_pcm_prepare(dac, 48000, 50, WRITE_MODE_BLOCK);
    sound_dac_power_setup(dac);
    /*sound_pcm_write_slience(dac, sound_pcm_current_ptr(dac), 1024);*/
    sound_pcm_start(dac);
    gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, 1);
    while (1) {
        os_time_dly(100);

        plnk_info_dump();
    };

}

#endif

#define PLNK_ADC_4MIC_DEMO_ENABLE	0
#if PLNK_ADC_4MIC_DEMO_ENABLE
/*
 * 纯 ADC 4MIC demo：
 * 4 路 PDM MIC 全部走 audio_adc DMIC 通道采集，不使用 PLNK 采样通路。
 * PLNK 模块仅提供 DMIC 的 SCLK 时钟和 DAT0/DAT1 数据线引脚。
 *
 * DMIC 映射：
 *   dmic0 = IO0 rising   dmic1 = IO0 falling
 *   dmic2 = IO1 rising   dmic3 = IO1 falling
 *
 * 硬件接法：两根数据线，各挂 2 个 MIC，共 4 MIC
 *   DAT0: MIC0(rising) + MIC2(falling)
 *   DAT1: MIC1(rising) + MIC3(falling)
 */

#define PLNK_ADC_4MIC_SR           16000   /* 采样率 */
#define PLNK_ADC_4MIC_IRQ_POINTS   256     /* 每次中断每通道点数 */
#define PLNK_ADC_4MIC_BUF_NUM      3       /* ping-pong 段数 */
#define PLNK_ADC_4MIC_CH           4       /* 通道数 */

static audio_plnk_t plnk_adc_4mic_cfg = {
    .sclk_io   = IO_PORTA_01,
    .ch0_io    = IO_PORTA_12,     /* DAT0: MIC0 + MIC2 */
    .ch1_io    = IO_PORTA_13,     /* DAT1: MIC1 + MIC3 */
    .sr        = PLNK_ADC_4MIC_SR,
    .buf_len   = PLNK_ADC_4MIC_IRQ_POINTS,
};

static struct adc_mic_ch plnk_adc_4mic_ch;
static struct audio_adc_output_hdl plnk_adc_4mic_output;
static s16 *plnk_adc_4mic_buf = NULL;
static s16 *plnk_adc_4mic_stereo = NULL;

/* audio_adc 输出回调：4 路 DMIC 交织数据 → 解交织回放 2 路到 DAC */
static void audio_plnk_adc_4mic_output(void *priv, s16 *data, int len)
{
    /* len = 每通道字节数 = done_points * 2 */
    int points = len / 2;   /* 每通道点数 */
    int i;
    s16 *dst = plnk_adc_4mic_stereo;

    /* printf("A:%d,%d,%d,%d,%d,%d,%d,%d\n",data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7]); */
    for (i = 0; i < points; i++) {
        /* data 是 4 通道交织: [m0,m1,m2,m3, m0,m1,m2,m3, ...] */
        /* 默认回放 DAT0 的 2 路 (m0/m1)；测 DAT1 改 data[i*4+2]/data[i*4+3] */
        /* dst[0] = data[i * 4 + 0]; */
        /* dst[1] = data[i * 4 + 1]; */
        dst[0] = data[i * 4 + 2];
        dst[1] = data[i * 4 + 3];

        dst += 2;
    }

    sound_pcm_write(dac, plnk_adc_4mic_stereo, points * 2 * sizeof(s16));
}

void audio_plnk_adc_4mic_test(void)
{
    y_printf("plnk adc 4mic demo\n");

    /* 复用 PLNK 模块提供 DMIC 的 SCLK 时钟，但不启用 PLNK 采样通路 */
    pdm_hdl = &plnk_adc_4mic_cfg;
    if (pdm_hdl->sr % 8000) {
        pdm_hdl->plink_sclk = 3000000;
    } else {
        pdm_hdl->plink_sclk = 2400000;
    }

    PLNK_CON_RESET();
    SFR(JL_PLNK->CON, 1, 1, 0);   /* CH0 数据及时钟来源：0=数字 MIC */
    SFR(JL_PLNK->CON, 5, 1, 0);   /* CH1 数据及时钟来源：0=数字 MIC */

    /* 配置 DMIC 时钟 + 数据线引脚 */
    audio_plnk_sr_set(pdm_hdl->sr);
    audio_plnk_sclk_io_init(pdm_hdl->sclk_io);
    audio_plnk_ch_io_init(0, pdm_hdl->ch0_io);
    audio_plnk_ch_io_init(1, pdm_hdl->ch1_io);
    SFR(JL_ASS->CLK_CON, 0, 2, 3);   /* 打开 PLNK 时钟 */

    /* 打开 4 个 DMIC 通道 */
    audio_adc_mic_open(&plnk_adc_4mic_ch, AUDIO_ADC_DMIC_0, &adc_hdl);
    audio_adc_mic_open(&plnk_adc_4mic_ch, AUDIO_ADC_DMIC_1, &adc_hdl);
    audio_adc_mic_open(&plnk_adc_4mic_ch, AUDIO_ADC_DMIC_2, &adc_hdl);
    audio_adc_mic_open(&plnk_adc_4mic_ch, AUDIO_ADC_DMIC_3, &adc_hdl);

    /* 设置采样率 + buffer */
    audio_adc_mic_set_sample_rate(&plnk_adc_4mic_ch, pdm_hdl->sr);
    plnk_adc_4mic_buf = (s16 *)zalloc(PLNK_ADC_4MIC_IRQ_POINTS * PLNK_ADC_4MIC_BUF_NUM * PLNK_ADC_4MIC_CH * sizeof(s16));
    ASSERT(plnk_adc_4mic_buf);
    plnk_adc_4mic_stereo = (s16 *)zalloc(PLNK_ADC_4MIC_IRQ_POINTS * 2 * sizeof(s16));
    ASSERT(plnk_adc_4mic_stereo);
    audio_adc_mic_set_buffs(&plnk_adc_4mic_ch, plnk_adc_4mic_buf,
                            PLNK_ADC_4MIC_IRQ_POINTS * 2, PLNK_ADC_4MIC_BUF_NUM);

    /* 输出回调 */
    plnk_adc_4mic_output.handler = audio_plnk_adc_4mic_output;
    audio_adc_add_output_handler(&adc_hdl, &plnk_adc_4mic_output);

    /* 打开 DAC 回放 */
    int err = sound_pcm_create(&dac, "dac", 0);
    if (err) {
        printf("Create sound pcm error.\n");
        return;
    }
    sound_pcm_ctl_ioctl(dac, SNDCTL_IOCTL_SET_BIAS_TRIM, 0);
    sound_pcm_set_irq_handler(dac, NULL, NULL);
    sound_pcm_prepare(dac, 16000, 50, WRITE_MODE_BLOCK);
    sound_dac_power_setup(dac);
    sound_pcm_start(dac);
    gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, 1);

    /* 启动 ADC 采集 */
    audio_adc_mic_start(&plnk_adc_4mic_ch);

    y_printf("plnk adc 4mic demo start ok\n");

    while (1) {
        os_time_dly(100);
    }
}
#endif

