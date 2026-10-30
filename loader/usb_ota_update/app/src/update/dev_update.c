#include "update_main.h"
#include "asm/power/p33.h"
#include "common.h"
#include "sd_card.h"
#include "tff.h"
#include "usb/usb_host.h"
#include "usb/usb_storage.h"
#include "jlfs.h"
#include "dec.h"
#include "asm/gpio.h"

#define LOG_TAG_CONST       DEV_UP
#define LOG_TAG             "[DEV_UPDATE]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if EX_NORFLASH_FATFS_MODULE_CONTROL
int dev_nor_flash_init(void);
#endif

extern void latch_unlock(void);

typedef struct {
    u16 dev_fs_mount; //挂载是否成功标志
    u16 dev_num; //设备类型
    struct sd_pdate_type sd_pdate; //sd 卡变量

    u32 mutil_ufw_offset; //多芯片升级方式
} _DEV_UPDATE_ARG;

_DEV_UPDATE_ARG dev_arg = {
    .dev_fs_mount = 1,
    .mutil_ufw_offset = 0,
};

typedef enum _DEV_STATUS {
    DEV_OFFLINE = 0,
    DEV_ONLINE,
    DEV_NULL,
} DEV_STATUS;

#if UPDATA_DEV_ONLINE_CHECK_EN
static DEV_STATUS sd0_dev_det(void)
{
#if SD_MODULE_CONTROL
    return sd_dev_det(SD_CONTROLLER_0);
#endif
    retrun DEV_NULL;
}

static DEV_STATUS sd1_dev_det(void)
{
#if SD_MODULE_CONTROL
    return sd_dev_det(SD_CONTROLLER_1);
#endif
    retrun DEV_NULL;
}


static DEV_STATUS usb_dev_det(void)
{
#if USB_HOST_MODULE_CONTROL
    return usb_online_check(5);
#endif
    retrun DEV_NULL;
}


static DEV_STATUS(*const dev_detect[MAX_DEVICE])(void) = {
    usb_dev_det,
    sd0_dev_det,
    sd1_dev_det,
};

#define DEV_CHECK_RETRY        10
#define DEV_CHECK_DELAY_CNT    2

static u16 dev_check_loop(u16 dev_type)
{
    u32 check_retry = 0;

    ///设备在线检测
    for (check_retry = 0; check_retry < DEV_CHECK_RETRY; check_retry++) {
        if (DEV_ONLINE == dev_detect[dev_type_2_index(dev_type)]()) { //相应设备检测
            deg_printf("\ncheck_dev_type=%d\n", dev_type);
            return dev_type;
        }
        delay_2ms(DEV_CHECK_DELAY_CNT);
    }

    return DEV_NULL; //无设备在线
}

static DEV_STATUS wait_dev_offline(u16 dev_type)
{
    return dev_detect[dev_type_2_index(dev_type)]();
}
#endif/*UPDATA_DEV_ONLINE_CHECK_EN*/

static u32 dev_mount_loop(u16 dev_type)
{
    u32 res = 0xff;

    switch (dev_type) {
#if USB_HOST_MODULE_CONTROL
    case USB_UPDATA:
        log_info("USB Insert...var_init\n");
        /* usb_host_status(1);             //设置USB在线 */
        /* res = usb_host_mount(3, 500, 20, 100, usb_data_buf); //usb host挂载,time out时间为5s */
        res = usb_host_mount(0, 3, 20, 300);
        usb_stor_init();
        break;
#endif

#if SD_MODULE_CONTROL
    case SD0_UPDATA:
    case SD1_UPDATA:
        log_info("SD Insert...var_init\n");
        res = sd_init_card(&dev_arg.sd_pdate);
        break;
#endif
#if EX_NORFLASH_FATFS_MODULE_CONTROL
    case DEV_NORFLASH_UFW_UPDATA:
        printf(">>>[test]:dev_nor_flash_init\n");
        res = dev_nor_flash_init();
        break;
#endif
    default:
        break;
    }

    return res;
}

static void dev_update_init(u32 dev_type)
{
    int res;
    ///1、设备检测
#if UPDATA_DEV_ONLINE_CHECK_EN
    dev_type = dev_check_loop(dev_type);
    if (DEV_NULL == dev_type) {
        log_info("DEV_NULL\n");
        goto __MOUNT_ERR;
    }
#endif/*online_dev_var_init(dev_type);*/

    ///2、设备挂载
    res = dev_mount_loop(dev_type);
    if (res) {
        log_info("DEV_MOUNT_ERR\n");
        goto __MOUNT_ERR;
    }

    ///3、文件系统挂载与打开文件测试
    res = fs_dev_mount(dev_type);
    if (res) {
        log_info("FS_MOUNT_ERR\n");
        goto __MOUNT_ERR;
    }
    dev_arg.dev_fs_mount = 0;
    return;
__MOUNT_ERR:
    dev_arg.dev_fs_mount = 1;
    return;
}

static void dev_update_parm_set(void *priv)
{
    UPDATA_PARM *p  = (UPDATA_PARM *)priv;
    dev_arg.dev_num = p->parm_type;
    printf(">>>[test]:dev_num = 0x%x\n", dev_arg.dev_num);

    if ((SD0_UPDATA == dev_arg.dev_num) || (SD1_UPDATA == dev_arg.dev_num)) { //复制已经在SDK鉴定过的SD卡变量

        memset((u8 *)&dev_arg.sd_pdate, 0x0, sizeof(struct sd_pdate_type));
        memcpy((u8 *)&dev_arg.sd_pdate, (void *)p->parm_priv, sizeof(struct sd_pdate_type));

        u8 *data = update_param_ext_get(p, EXT_SD_IO_INFO);
        if (data) {
            dev_arg.sd_pdate.control_io_clk = get_gpio(&data[0]);
            dev_arg.sd_pdate.control_io_cmd = get_gpio(&data[4]);
            dev_arg.sd_pdate.control_io_dat = get_gpio(&data[8]);
            if (dev_arg.sd_pdate.control_io_clk > IO_PORT_MAX || dev_arg.sd_pdate.control_io_cmd > IO_PORT_MAX || dev_arg.sd_pdate.control_io_dat > IO_PORT_MAX) {
                dev_arg.sd_pdate.control_io_clk = get_gpio(&data[0]);
                dev_arg.sd_pdate.control_io_cmd = get_gpio(&data[5]);
                dev_arg.sd_pdate.control_io_dat = get_gpio(&data[10]);
            }
        }
        log_info_hexdump((u8 *)(&dev_arg.sd_pdate), sizeof(struct sd_pdate_type));
    } else if (USB_UPDATA == dev_arg.dev_num) {
        log_info("USB_DEV_UPDATA\n");
        /* memset((u8 *)&g_usb_var, 0x0, sizeof(sUSB_VAR)); */
    } else if (DEV_NORFLASH_UFW_UPDATA == dev_arg.dev_num) {
        log_info("DEV_NOR_FATFS_UPDATA\n");
    }

}

static u16 dev_f_open(void)
{
    return dev_arg.dev_fs_mount;
    /* return f_open(); */
}

static u16 dev_f_read(void *fp, void *buf, u32 rlen)
{
    u32 len;
    wdt_clr();
    len = f_read(fp, buf, rlen);
    return len;
}

static int dev_f_seek(void *fp, u8 type, u32 offset)
{
    if (type == SEEK_SET) {
        offset += dev_arg.mutil_ufw_offset;
    }
    return f_seek(fp, type, offset);//FR_OK;
}


void mutil_cpu_set_offset(u32 offset)
{
    dev_arg.mutil_ufw_offset = offset;
    dev_f_seek(dev_update_handle_get(), SEEK_SET, 0); //预先设置好偏移
}


static void dev_update_state_cbk(u32 status, void *priv)
{
    switch (status) {
    case UPDATE_PARM:       //升级需要的参数
        log_info("DEV_UPDATE_SPARM_SET...\n");
        dev_update_parm_set(priv);
        break;
    case UPDATE_START:      //测试盒edr升级需要根据priv判断是否恢复基带
        log_info("DEV_UPDATE_START...\n");
        dev_update_init(dev_arg.dev_num);
        break;
    case UPDATE_END:		//升级结束需要保存结果到Ram给SDK获取，并回复主机升级结果
        if (*((u8 *)priv) == UPDATE_ERR_NONE) {
            set_updata_result(dev_arg.dev_num, UPDATA_SUCCESSFULLY);
        } else {
            set_updata_result(dev_arg.dev_num, UPDATA_DEV_ERR);
        }
        /* cpu_reset(); */
        update_reset();
        break;
    }
}


update_op_api_t dev_op_api = {
    .f_open = dev_f_open,
    .f_read = dev_f_read,
    .f_seek = dev_f_seek,
    .notify_update_content_size = NULL,
};

update_mode_info_t update_mode_info = {
    .type      = DEV_UPDATE,
    .state_cbk = dev_update_state_cbk,
    .file_op   = &dev_op_api,
};

