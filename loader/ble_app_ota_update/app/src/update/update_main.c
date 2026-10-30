#include "common.h"
#include "asm/crc.h"
#include "update_main.h"
#include "lib_include.h"
#include "asm/power/p33.h"
#include "dec.h"
#include "jlfs.h"
#include "asm/gpio.h"


#define LOG_TAG_CONST       UPDATE_MAIN
#define LOG_TAG             "[UPDATE_MAIN]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
#define LOG_DUMP_ENABLE
#define LOG_CLI_ENABLE
#include "debug.h"

extern update_mode_info_t update_mode_info;
extern void update_get_record_form_flash(u8 *ram);
extern u8 *update_param_ext_get(UPDATA_PARM *p, u8 ext_type);
extern int update_loop(update_op_api_t *update_op);
extern void ex_api_start_show(u8 state);
extern u8 ex_api_code_load(u32 opt);
extern void reserved_area_update_file_disable(void);
extern int func_set_rf_info(u8 *buf);

__attribute__((weak)) int func_set_rf_info(u8 *buf)
{
    return 0;
}

extern void latch_unlock(void);

void ram_protect_close(void);
void bredr_bd_close();
void ll_hci_destory(void);
void update_enter_jump_maskrom(void);
void latch_reset(void);
static u8 latch_flag = 0;
void jump_mode_check(u8 *p);

#define __this  (&update_mode_info)

// 从扩展升级参数中取出信息，判断是否不升级升级预留区域
static u8 reserved_update_check(u8 *p)
{
    u8 *reserved_zone_update_flag = update_param_ext_get(p, EXT_RESERVED_UPDATE);
    if (reserved_zone_update_flag && reserved_zone_update_flag[0]) {
        reserved_area_update_file_disable();
    }
    return 0;
}

//检查ram里的update信息
bool update_check(void)
{
    u16 crc_cal;
    //check_updata_parm is valid
    UPDATA_PARM *p  = (u8 *)UPDATE_FLAG_ADDR; //定位到ram1最后地址
    log_info("update param:");
    log_info_hexdump(p, sizeof(UPDATA_PARM));
    log_info("update param priv:");
    log_info_hexdump(p->parm_priv, 32);
    crc_cal = chip_crc16(((u8 *)p) + 2, sizeof(UPDATA_PARM) - 2);
    if (crc_cal && crc_cal == p->parm_crc) {
        if ((NON_DEV_UPDATA != p->parm_type) && (UPDATA_READY == p->parm_result)) {
            log_info("update_check ture\n");
            return TRUE;
        }
    }
    return FALSE;
}

//写升级标志到ram to SDK, 通知SDK升级结果
void set_updata_result(u16 type, u16 result)
{
    UPDATA_PARM *p;

    log_info("set update result :%d %d\n", type, result);
    p = (u8 *)UPDATE_FLAG_ADDR;
    memset(p, 0x0, sizeof(UPDATA_PARM));
    p->parm_result = result;
    p->magic = type;
    p->parm_crc = chip_crc16(((u8 *)p) + 2, sizeof(UPDATA_PARM) - 2);
}

#define LATCH_IO_NUM  10
/* static u32 latch_io[LATCH_IO_NUM] = {0}; */
static u8 val[LATCH_IO_NUM * 7 + 1] = {0};
//格式为PB01&0_PB02&1  &0\1表示输出低或者高

static void mutual_get_update_latch_io(u8 *pos)
{
    if (!pos) {
        return;
    }
    for (int i = 0; i < 4; i++) {
        u32 keep_io = get_gpio(&pos[i * 5]);
        printf(" latch_io[%d] = %d, %s\n", i, keep_io, &pos[i * 5]);
        if (keep_io < IO_PORT_MAX) {
            gpio_set_direction(keep_io, 0);
            gpio_set_output_value(keep_io, 1);
        }
    }
}

static u32 update_set_latch_io(void)
{
    u8 *ptr = jlfs_get_isd_cfg_ptr();
    if (dec_isd_cfg_ini("SD_LATCH_IO", (void *)val, ptr)) {
        u8 *pos = &val[0];
        mutual_get_update_latch_io(pos);
        return 0;
    }
    if (dec_isd_cfg_ini("LATCH_IO", (void *)val, ptr)) {
        put_buf(val, sizeof(val));
        int len = strlen(val);

        int i = 0;
        u8 *val_s = &val[0];
        u8 *pos = &val[0];
        u8 io_arg[10];
        while (1) {
            if (len < i * 7) {
                break;
            }
            //获取‘&’的位置,获取高低电平设置
            u8 *level = strchr(pos, '&');
            if (level == NULL) {
                mutual_get_update_latch_io(pos);
                break;
            }
            u8 value = (u8)(level[1] - '0');

            //获取IO的字符串，传给gpio();
            u32 io_len = (u32)(level - pos);
            memset(io_arg, 0, sizeof(io_arg));
            memcpy(io_arg, pos, io_len);
            u32 keep_io = get_gpio(io_arg);

            //设置IO状态
            printf(" latch_io[%d] = %d, %s,  value = %d\n", i, keep_io, io_arg, value);
#ifdef IO_PORT_USB_MASK
            if (keep_io == IO_PORT_DP || keep_io == IO_PORT_DM) {
                gpio_set_direction(keep_io, 1);
                gpio_set_pull_up(keep_io, value);
                gpio_set_pull_down(keep_io, !value);
            } else
#endif
            {
                gpio_set_direction(keep_io, 0);
                gpio_set_output_value(keep_io, value);
            }

            //基指针偏移
            pos = strchr(pos, '_');
            if (pos == NULL) {
                break;
            }
            pos ++;
            i++;
        }
        return i;
    } else {
        return -1;
    }
}


static void update_set_latch()
{
    if (update_set_latch_io() != -1) {
        latch_unlock();
    }
}

void update_main()
{
    UPDATA_PARM *p  = (u8 *)UPDATE_FLAG_ADDR;
    ex_api_code_load(0);
    ex_api_start_show(UPDATA_READY == p->parm_result);
    UPDATE_MODE mode = UPDATE_JUMP;
    UPDATA_RESULT result = UPDATA_RESULT_FAIL;
#if !UART_UPDATE_ONLY_TEST_MODE
    //update_check
    if (update_check() == FALSE) {			                   //RAM中的结构体不存在， 判断为断电重新上电
        log_info("Ota Start PowerOn\n");
        mode = UPDATE_POWERON;
    }

    update_get_record_form_flash(&p);       //从flash里把参数读出来
    u8 *data = NULL;
#if defined(CONFIG_CPU_BR28)
    data = update_param_ext_get(p, EXT_CHARGE_LEVEL);
    if (data) {
        printf("%s, %d, %x\n", __func__, __LINE__, data[0]);
        void loader_charge_init(u8 level);
        loader_charge_init(data[0]);
    }
#endif

    data = update_param_ext_get(p, EXT_RF_PA_INFO);
    func_set_rf_info(data);

#endif

    jump_mode_check(p);

    reserved_update_check(p);

    update_set_latch();

    /* mode = UPDATE_POWERON; */
    //给各升级模块传递参数
    __this->state_cbk(UPDATE_PARM, p);

    if (__this->type == UART_UPDATA) {
        u32 buf[2];
        buf[0] = mode;
        memcpy((u8 *)&buf[1], p->parm_priv, sizeof(u32));
        __this->state_cbk(UPDATE_START, buf);
    } else {
        __this->state_cbk(UPDATE_START, &mode);
    }
#if !UART_UPDATE_ONLY_TEST_MODE
    //loop
    result = update_loop(__this->file_op);

    //end
    __this->state_cbk(UPDATE_END,  &result);                    //不同升级完成之后可能需要执行不同操作
#endif
}



void uart_close_all(void)
{
    JL_UART0->CON0 = 0;
    JL_UART1->CON0 = 0;
    JL_UART2->CON0 = 0;
}


u32 get_update_jump_flag()
{
    u8 *ptr = jlfs_get_isd_cfg_ptr();
    u32 u32Val;
    u8 val[16] = {0};
    printf("\n >>>[test]:func = %s,line= %d\n", __FUNCTION__, __LINE__);
    if (dec_isd_cfg_ini("UPDATE_JUMP", (void *)val, ptr)) {
        put_buf(val, 16);
        /* u32Val = (u32) (((u8 *)val)[0] - '0'); */
        u32Val = (u32)val[0];
        printf("update_jump = %d, vol[0] = %d\n", u32Val, val[0]);
        return u32Val;
    } else {
        return 0;
    }
}

void jump_mode_check(u8 *p)
{
    u8 *p_latch_flag = NULL;
    if (latch_flag == 0) {
        p_latch_flag = update_param_ext_get((UPDATA_PARM *)p, EXT_JUMP_FLAG);
        if (p_latch_flag) {
            latch_flag = p_latch_flag[0];
        }
    }
    printf(">>>[test]:latch_flag = %d\n", latch_flag);
}

void update_reset(void)
{
    printf("\n >>>[test]:func = %s,line= %d\n", __FUNCTION__, __LINE__);
    /* 寄存器不会复位 */
#if (0 == UART_UPDATE_ONLY_TEST_MODE)
    /* u8 *p = NULL; */
    /* UPDATA_PARM *p  = (UPDATA_PARM *)UPDATE_FLAG_ADDR; */
    /* update_get_record_form_flash((u8 *)&p);       //从flash里把参数读出来 */
    /* jump_mode_check(p); */
    if (latch_flag) {
        puts("latch reset...\n");
        void latch_reset(void);
        latch_reset();
    } else
#endif
    {
        /* uart_close_all(); */
        cpu_reset();
    }
}

