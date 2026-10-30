
#include "app_config.h"
#include "media/includes.h"
#include "audio_config.h"
#include "audio_way.h"
#include "audio_app_stream.h"
#include "audio_dec.h"


#ifndef AUDIO_OUT_WAY_TYPE
#error "no defined AUDIO_OUT_WAY_TYPE"
#endif
//提示音走DAC叠加相关函数定义使能
//功能生效需要在lib_media_config.c里面把audio_dec_app_mix_en清0
#define TONE_SUPPORT_DAC_MIX        1

extern const int audio_dec_app_mix_en;
struct audio_app_stream {
    struct audio_stream_entry entry;
    u32 out_way;
    u32 cur_sr;
    u8  start;
};

static struct audio_app_stream 	_audio_stream;

void audio_way_resume(void)
{
    /* putchar('r'); */
    if (_audio_stream.start) {
        audio_stream_resume(&_audio_stream.entry);
    }
}
/*
 *br28 dac初始化耗时120ms,提前将dac初始化，避免出sniff后，蓝牙通路耗时过长，导致同步将开始的声音调没的,缺字的情况
 * */
void audio_way_try_power_on()
{
    audio_way_ioctrl(_audio_stream.out_way, SNDCTL_IOCTL_TRY_POWER_ON, NULL);
}
static int audio_app_stream_data_handler(struct audio_stream_entry *entry,
        struct audio_data_frame *in,
        struct audio_data_frame *out)
{
    struct audio_app_stream *stream = container_of(entry, struct audio_app_stream, entry);
    AUDIO_WAY_PEND();
    if (in->stop) {
        if (stream->start) {
            stream->start = 0;
#if defined(TCFG_AUDIO_DAC_PA_PORT) && (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT)
            if (_audio_stream.out_way & AUDIO_WAY_TYPE_DAC && audio_dec_app_mix_en) { //如果提示音走dac叠加,则不能关功放
                gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, 0);
            }
#endif /* #if (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT) */
            audio_way_stop(_audio_stream.out_way);
            /* audio_way_ioctrl(_audio_stream.out_way, SNDCTL_IOCTL_POWER_OFF, NULL); */
            /* audio_way_close(_audio_stream.out_way); */
        }
        AUDIO_WAY_POST();
        return 0;
    } else {
        if (stream->start == 0) {
            stream->start = 1;
            stream->cur_sr = in->sample_rate;
            /* audio_way_open(_audio_stream.out_way); */
            audio_way_set_sample_rate(_audio_stream.out_way, stream->cur_sr);
            audio_way_ioctrl(_audio_stream.out_way, SNDCTL_IOCTL_POWER_ON, NULL);
            u8 volume = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);

#if !PC_VOL_INDEPENDENT_EN
            app_audio_set_volume(APP_AUDIO_CURRENT_STATE, volume, 1);
#else
            u8 volume_r = app_audio_get_volume_r(APP_AUDIO_CURRENT_STATE);
            app_audio_set_volume_each_channel(APP_AUDIO_CURRENT_STATE, volume, volume_r, 1);
#endif
            /* audio_way_set_gain(_audio_stream.out_way, volume); */
            audio_way_start(_audio_stream.out_way);
#if defined(TCFG_AUDIO_DAC_PA_PORT) && (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT)
            if (_audio_stream.out_way & AUDIO_WAY_TYPE_DAC) {
                gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, 1);
            }
#endif /* #if (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT) */
        }
        if (stream->cur_sr != in->sample_rate) {
            stream->cur_sr = in->sample_rate;
            audio_way_set_sample_rate(_audio_stream.out_way, stream->cur_sr);
        }
    }
    if (in->data_len == 0) {
        AUDIO_WAY_POST();
        return 0;
    }
#if TCFG_USER_EMITTER_ENABLE && (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_BT)
    if (_audio_stream.out_way & AUDIO_WAY_TYPE_DAC) {
        extern u8 bt_emitter_audio_get_mute();
        if (bt_emitter_audio_get_mute()) {
            memset(in->data, 0, in->data_len);
        }
    }
#endif /* #if (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_BT) */
    int wlen = audio_way_output_write(in->data, in->data_len);
    AUDIO_WAY_POST();
    return wlen;
}

int audio_app_stream_switch_way(u32 close_way, u32 open_way)
{
    AUDIO_WAY_PEND();
    if (close_way) {
        audio_way_close(close_way);
        _audio_stream.out_way &= (~close_way);
#if defined(TCFG_AUDIO_DAC_PA_PORT) && (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT)
        if (close_way & AUDIO_WAY_TYPE_DAC && audio_dec_app_mix_en) {
            gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, 0);
        }
#endif /* #if (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT) */
    }
    if (open_way) {
        audio_way_open(open_way);
        _audio_stream.out_way |= open_way;
    }
    _audio_stream.start = 0;
    audio_stream_resume(&_audio_stream.entry);
    AUDIO_WAY_POST();
    y_printf("switch way:0x%x \n", _audio_stream.out_way);
    return 0;
}

#if TCFG_UI_ENABLE
#include "ui/ui_sys_param.h"
extern u8 bt_phone_dec_is_running();
static int audio_app_stream_prob_handler(struct audio_stream_entry *entry,  struct audio_data_frame *in)
{
    if (in && in->data && in->data_len) {
        if (bt_phone_dec_is_running()) {
            // 通话不使用全局静音
        } else {
            if (get_ui_sys_param(SysVoiceMute)) {
                memset(in->data, 0, in->data_len);
            }
        }
    }
    return 0;
}
#endif /* #if TCFG_UI_ENABLE */

void audio_app_stream_init(void)
{
    memset(&_audio_stream, 0, sizeof(struct audio_app_stream));
#if TCFG_UI_ENABLE
    _audio_stream.entry.prob_handler = audio_app_stream_prob_handler;
#endif /* #if TCFG_UI_ENABLE */
    _audio_stream.entry.data_handler = audio_app_stream_data_handler;
    _audio_stream.out_way = AUDIO_OUT_WAY_TYPE;
    /* audio_way_open(AUDIO_OUT_WAY_TYPE); */
}

struct audio_stream_entry *audio_app_stream_get_entry(void)
{
    return &_audio_stream.entry;
}

u32 audio_app_stream_get_out_way(void)
{
    return _audio_stream.out_way;
}

/*****************************************************************************/
struct sound_pcm_stream *reverb_dac = NULL;
static struct audio_app_stream 	_audio_stream_dac;
static int audio_app_stream_dac_data_handler(struct audio_stream_entry *entry,
        struct audio_data_frame *in,
        struct audio_data_frame *out)
{

    struct audio_app_stream *stream = container_of(entry, struct audio_app_stream, entry);

    if (stream->start == 0) {
        stream->start = 1;

        struct sound_volume volume = {
            .chmap = SOUND_CHMAP_FL | SOUND_CHMAP_FR,
        };
        // 模拟音量
        u8 gain = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
        volume.volume[0] = gain;
        volume.volume[1] = gain;
        sound_pcm_ctl_ioctl(reverb_dac, SNDCTL_IOCTL_SET_ANA_GAIN, &volume);
        // 数字音量
        volume.volume[0] = 16384;
        volume.volume[1] = 16384;
        sound_pcm_ctl_ioctl(reverb_dac, SNDCTL_IOCTL_SET_DIG_GAIN, &volume);
        /* printf("JL_AUDIO->DAC_VL0 : 0x%x, \n", JL_AUDIO->DAC_VL0); */
        //sample_rate
        sound_pcm_prepare(reverb_dac, MIC_EFFECT_SAMPLERATE, 10, WRITE_MODE_FORCE);
        sound_pcm_ctl_ioctl(reverb_dac, SNDCTL_IOCTL_POWER_ON, NULL);
        sound_pcm_start(reverb_dac);
        printf("-----------sound dac/iis start\n");
    }
    if (in->data_len) {
        static u16 tx_s_cnt = 0;
        /* get_sine_data(&tx_s_cnt, in->data,in->data_len/2, 1); */
        /* putchar('o'); */
        /* put_buf(in->data, in->data_len); */
        sound_pcm_write(reverb_dac, in->data, in->data_len);
    }
    return in->data_len;
}
void audio_reverb_stream_dac_init(void)
{
    memset(&_audio_stream_dac, 0, sizeof(struct audio_app_stream));
    _audio_stream_dac.entry.data_handler = audio_app_stream_dac_data_handler;
    _audio_stream_dac.out_way = AUDIO_OUT_WAY_TYPE;

#if (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_DAC)
    int err = sound_pcm_create(&reverb_dac, "dac", 0);
#endif

#if (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_IIS)
    int err = sound_pcm_create(&reverb_dac, "iis", 0);
#endif

}



struct audio_stream_entry *audio_reverb_stream_dac_get_entry(void)
{
    return &_audio_stream_dac.entry;
}

void audio_reverb_stream_dac_uninit(void)
{
    sound_pcm_stop(reverb_dac);
    audio_stream_del_entry(&_audio_stream_dac.entry);
    sound_pcm_ctl_ioctl(reverb_dac, SNDCTL_IOCTL_POWER_OFF, NULL);
    sound_pcm_free(reverb_dac);
}

#if	TONE_SUPPORT_DAC_MIX
static  struct audio_app_stream *g_tone_stream;
#define TONE_DAC_DEFAULT_SR  (44100)
static struct sound_pcm_stream *tone_dac = NULL;
static struct audio_app_stream 	tone_stream_dac;
static void *tone_src;		// 变采样
static u8 tone_need_resume = 0;
static u16 tone_timer = 0;
void audio_tone_stream_dac_uninit(void);
static int audio_src_output_handler(void *hdl, void *buf, int len)
{

    struct audio_way *p = hdl;
    if (tone_dac) {
        int wlen = sound_pcm_write(tone_dac, buf, len);
        if (wlen < 0) {
            wlen = 0;
        }
        if (tone_need_resume && wlen) {
            if (g_tone_stream && g_tone_stream->start) {
                audio_stream_resume(&(g_tone_stream->entry));
            }
            tone_need_resume = 0;
        }
        return wlen;
    } else {
        return 0;
    }
}
void tone_dac_wakeup_irq_handler(void *priv)
{
    if (g_tone_stream && g_tone_stream->start) {
        audio_stream_resume(&(g_tone_stream->entry));
    }
}

static void tone_dac_resume(void)
{
    tone_timer = 0;
    if (g_tone_stream && g_tone_stream->start) {
        audio_stream_resume(&(g_tone_stream->entry));
    }
}

static int audio_tone_stream_dac_data_handler(struct audio_stream_entry *entry,
        struct audio_data_frame *in,
        struct audio_data_frame *out)
{
    struct audio_app_stream *stream = container_of(entry, struct audio_app_stream, entry);
    g_tone_stream = stream;
    if (in->stop) {
        if (stream->start) {
            stream->start = 0;
            audio_tone_stream_dac_uninit();
        }
        return 0;
    }
    if (stream->start == 0) {
        stream->start = 1;
#if defined(TCFG_AUDIO_DAC_PA_PORT) && (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT)
        gpio_direction_output(TCFG_AUDIO_DAC_PA_PORT, 1);
#endif /* #if (TCFG_AUDIO_DAC_PA_PORT != NO_CONFIG_PORT) */
        u8 volume = app_audio_get_volume(APP_AUDIO_CURRENT_STATE);
#if !PC_VOL_INDEPENDENT_EN
        app_audio_set_volume(APP_AUDIO_CURRENT_STATE, volume, 1);
#else
        u8 volume_r = app_audio_get_volume_r(APP_AUDIO_CURRENT_STATE);
        app_audio_set_volume_each_channel(APP_AUDIO_CURRENT_STATE, volume, volume_r, 1);
#endif
        int sr = audio_output_nor_rate();
        if (!sr) {
            sr = app_audio_output_samplerate_get();
            if (sr <= 0) {
                sr = TONE_DAC_DEFAULT_SR;
            }
        }
        sound_pcm_prepare(tone_dac, sr, 50, WRITE_MODE_BLOCK);
        sound_pcm_ctl_ioctl(tone_dac, SNDCTL_IOCTL_POWER_ON, NULL);
        sound_pcm_set_irq_handler(tone_dac, NULL, tone_dac_wakeup_irq_handler);
        sound_pcm_start(tone_dac);
        printf("-----------sound dac/iis start\n");
    }
    int wlen = 0;
    if (in->data_len) {
        if (tone_src) {
            wlen = audio_src_resample_write(tone_src, in->data, in->data_len);
        } else {
            wlen = sound_pcm_write(tone_dac, in->data, in->data_len);
        }
    }
    if (wlen != in->data_len) {
        tone_need_resume = 1;
        if (!tone_timer) {
            tone_timer = sys_timeout_add(NULL, tone_dac_resume, 20);
        }
    }
    return wlen;
}

int tone_dac_run_stop(struct audio_data_frame *frame)
{
    if (tone_stream_dac.entry.data_handler) {
        return tone_stream_dac.entry.data_handler(&(tone_stream_dac.entry), frame, NULL);
    } else {
        return 0;
    }
}

void audio_tone_stream_dac_init(u32 in_sr)
{
    memset(&tone_stream_dac, 0, sizeof(struct audio_app_stream));
    tone_stream_dac.entry.data_handler = audio_tone_stream_dac_data_handler;
    tone_stream_dac.out_way = AUDIO_OUT_WAY_TYPE;
    int out_sr = audio_output_nor_rate();
    if (!out_sr) {
        out_sr = app_audio_output_samplerate_get();
        if (out_sr <= 0) {
            out_sr = TONE_DAC_DEFAULT_SR;
        }
    }
    if (in_sr != out_sr) {
        if (tone_src) {
            audio_hw_src_stop(tone_src);
            audio_hw_src_close(tone_src);
            free(tone_src);
            tone_src = NULL;
        }
        tone_src = zalloc(sizeof(struct audio_src_handle));
        ASSERT(tone_src);
        audio_hw_src_open(tone_src, 1, SRC_TYPE_RESAMPLE);
        audio_src_set_output_handler(tone_src, NULL, audio_src_output_handler);
        audio_hw_src_set_rate(tone_src, in_sr, out_sr);
    }

    if (!tone_dac) {
#if (AUDIO_OUT_WAY_TYPE & AUDIO_WAY_TYPE_DAC)
        int err = sound_pcm_create(&tone_dac, "dac", 0);
#endif
    }
}



struct audio_stream_entry *audio_tone_stream_dac_get_entry(void)
{
    return &tone_stream_dac.entry;
}

void audio_tone_stream_dac_uninit(void)
{
    g_tone_stream = NULL;
    if (tone_dac) {
        sound_pcm_stop(tone_dac);
        audio_stream_del_entry(&tone_stream_dac.entry);
        sound_pcm_free(tone_dac);
        tone_dac = NULL;
    }
    if (tone_src) {
        audio_hw_src_stop(tone_src);
        audio_hw_src_close(tone_src);
        free(tone_src);
        tone_src = NULL;
    }
}
#endif
