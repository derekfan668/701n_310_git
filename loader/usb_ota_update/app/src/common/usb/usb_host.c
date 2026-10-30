
/* #include "os/os_compat.h" */
/* #if TCFG_UDISK_ENABLE || TCFG_ADB_ENABLE ||TCFG_AOA_ENABLE || TCFG_HID_HOST_ENABLE */
#if 1
#include "usb/usb_host.h"
#include "asm/uart.h"
#include "usb/usb_phy.h"
#include "usb/usb_ctrl_transfer.h"
#include "usb/usb_storage.h"
#include "asm/clock.h"
#include "asm/csfr.h"

static struct usb_host_device host_devices[USB_MAX_HW_NUM];// SEC(.usb_h_bss);

#define     device_to_usbdev(device)	((struct usb_host_device *)((device)->private_data))

u32 host_device2id(const struct usb_host_device *host_dev)
{
#if USB_MAX_HW_NUM > 1
    return ((host_dev)->private_data.usb_id);
#else
    return 0;
#endif
}
struct usb_host_device *host_id2device(const usb_dev id)
{
#if USB_MAX_HW_NUM > 1
    return &host_devices[id];
#else
    return &host_devices[0];
#endif
}

/**
 * @brief usb_descriptor_parser
 *
 * @param device
 * @param pBuf
 * @param total_len
 *
 * @return
 */
static int _usb_msd_parser(struct usb_host_device *host_dev, u8 interface_num, const u8 *pBuf)
{
    usb_printf("find udisk @ interface %d\n", interface_num);
#if 1//TCFG_UDISK_ENABLE
    return   usb_msd_parser(host_dev, interface_num, pBuf);
#else
    return USB_DT_INTERFACE_SIZE;
#endif
}
static int usb_descriptor_parser(struct usb_host_device *host_dev, const u8 *pBuf, u32 total_len, struct usb_device_descriptor *device_desc)
{
    int len = 0;
    u8 interface_num = 0;
    struct usb_private_data *private_data = &host_dev->private_data;

    struct usb_config_descriptor *cfg_desc = (struct usb_config_descriptor *)pBuf;

    if (cfg_desc->bDescriptorType != USB_DT_CONFIG ||
        cfg_desc->bLength < USB_DT_CONFIG_SIZE) {
        usb_printf("invalid descriptor for config bDescriptorType = %d bLength= %d",
                   cfg_desc->bDescriptorType, cfg_desc->bLength);
        return -USB_DT_CONFIG;
    }

    usb_printf("idVendor %x idProduct %x\n", device_desc->idVendor, device_desc->idProduct);

    len += USB_DT_CONFIG_SIZE;
    pBuf += USB_DT_CONFIG_SIZE;
    int i = 0;
    u32 have_find_valid_class = 0;
    while (len < total_len) {
        if (interface_num > 4) {
            usb_printf("interface_num too much");
            break;
        }

        struct usb_interface_descriptor *interface = (struct usb_interface_descriptor *)pBuf;
        if (interface->bDescriptorType == USB_DT_INTERFACE) {

            usb_printf("inf class %x subclass %x ep %d\n",
                       interface->bInterfaceClass, interface->bInterfaceSubClass, interface->bNumEndpoints);

            if (interface->bInterfaceClass == USB_CLASS_MASS_STORAGE) {
                i = _usb_msd_parser(host_dev, interface_num, pBuf);
                if (i < 0) {
                    usb_printf("---%s %d---\n", __func__, __LINE__);
                    len = total_len;
                } else {
                    interface_num++;
                    len += i;
                    pBuf += i;
                    have_find_valid_class = 1;
                }
            } else {
                usb_printf("find unsupport [class %x subClass %x] @ interface %d\n",
                           interface->bInterfaceClass,
                           interface->bInterfaceSubClass,
                           interface_num);

                len += USB_DT_INTERFACE_SIZE;
                pBuf += USB_DT_INTERFACE_SIZE;
            }
        } else {
            /* usb_printf("unknown section %d %d", len, pBuf[0]); */
            if (pBuf[0]) {
                len += pBuf[0];
                pBuf += pBuf[0];
            } else {
                len = total_len;
            }
        }
    }

    usb_printf("len %d total_len %d\n", len, total_len);
    return !have_find_valid_class;
}

static u32 _usb_host_mount(const usb_dev usb_id, u32 retry, u32 reset_delay, u32 mount_timeout)
{
    u32 ret = 0;
    struct usb_host_device *host_dev = &host_devices[usb_id];
    struct usb_private_data *private_data = &host_dev->private_data;


    for (int i = 0; i < retry; i++) {
        usb_h_sie_init(usb_id);
        ret = usb_host_init(usb_id, reset_delay, mount_timeout);
        if (ret) {
            reset_delay += 10;
            continue;
        }

        void *const ep0_dma = usb_h_get_ep_buffer(usb_id, 0);
        usb_set_dma_taddr(usb_id, 0, ep0_dma);

        usb_sie_enable(usb_id);//enable sie intr
        usb_mdelay(reset_delay);

        /**********get device descriptor*********/
        struct usb_device_descriptor device_desc;
        private_data->usb_id = usb_id;
        /* private_data->status = 0; */
        private_data->devnum = 0;
        private_data->ep0_max_packet_size = 8;
        ret = usb_get_device_descriptor(host_dev, &device_desc);

        /**********set address*********/
        usb_mdelay(20);
        u8 devnum = 8;//rand32() % 16 + 1;
        ret = set_address(host_dev, devnum);
        check_usb_mount(ret);
        private_data->devnum  = devnum;

        /**********get device descriptor*********/
        usb_mdelay(20);
        ret = usb_get_device_descriptor(host_dev, &device_desc);
        check_usb_mount(ret);
        private_data->ep0_max_packet_size = device_desc.bMaxPacketSize0;

        /**********get config descriptor*********/
        struct usb_config_descriptor cfg_desc;
        ret = get_config_descriptor(host_dev, &cfg_desc, USB_DT_CONFIG_SIZE);
        check_usb_mount(ret);

        u8 desc_buf[128] = {0};
        cfg_desc.wTotalLength = min(sizeof(desc_buf), cfg_desc.wTotalLength);

        ret = get_config_descriptor(host_dev, desc_buf, cfg_desc.wTotalLength);
        check_usb_mount(ret);

        /**********set configuration*********/
        ret = set_configuration(host_dev);
        /* log_info_buf(desc_buf, cfg_desc.wTotalLength); */
        ret |= usb_descriptor_parser(host_dev, desc_buf, cfg_desc.wTotalLength, &device_desc);
        check_usb_mount(ret);

        break;//succ
    }

    if (ret) {
        goto __exit_fail;
    }
    /* private_data->status = 1; */
    return 0;

__exit_fail:
    usb_printf("usb_probe fail");
    /* private_data->status = 0; */
    /* usb_sie_close(usb_id); */
    return ret;
}

/* --------------------------------------------------------------------------*/
/**
 * @brief usb_host_mount
 *
 * @param usb
 *
 * @return
 */
/* --------------------------------------------------------------------------*/
extern void lrc_trace_trim(void);
u32 usb_host_mount(const usb_dev id, u32 retry, u32 reset_delay, u32 mount_timeout)
{
#if USB_MAX_HW_NUM > 1
    const usb_dev usb_id = id;
#else
    const usb_dev usb_id = 0;
#endif

    u32 ret = 0;
    struct usb_host_device *host_dev = &host_devices[usb_id];
    /* memset(host_dev, 0, sizeof(*host_dev)); */

    host_dev->private_data.usb_id = id;

    usb_host_config(usb_id);
    /* usb_h_isr_reg(usb_id, 3, 0); */
#if 0
    lrc_trace_trim();

    ret = 1;
    const u32 pll_ds = (JL_PLL->CON1 & 0xfff) + 2;
    if (1) {
        const u8 pll_cfg_cnt = 12;
        for (u8 i = 0; (i < pll_cfg_cnt) && (ret); i ++) {
            /* wdt_clear(); */
            /* if ((usb_otg_online(0) != HOST_MODE) || (ret == -DEV_ERR_OFFLINE) || (ret == -DEV_ERR_TIMEOUT)) { */
            /* break; */
            /* } */
            ret = _usb_host_mount(usb_id, retry, reset_delay, mount_timeout);
            if (ret) {
                if (i & 0x01) {
                    pll_config(pll_ds + (i >> 1) + 1);
                } else {
                    pll_config(pll_ds - (i >> 1) - 1);
                }
            } else {
                if (i > 0) {
                    if (i & 0x01) {
                        pll_config(pll_ds - ((i >> 1) + 1) - 3);
                    } else {
                        pll_config(pll_ds + (i >> 1) + 3);
                    }
                }
            }
        }
    } else {
        ret = _usb_host_mount(usb_id, retry, reset_delay, mount_timeout);
    }
#else
    ret = _usb_host_mount(usb_id, retry, reset_delay, mount_timeout);
#endif
    /* usb_otg_resume(usb_id);  //打开usb host之后恢复otg检测 */
    if (ret) {
        goto __exit_fail;
    }
    return 0;

__exit_fail:
    /* usb_sie_disable(usb_id); */
    return ret;
}

#endif
