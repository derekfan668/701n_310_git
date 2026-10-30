#include "usb/usb_ctrl_transfer.h"
#include "usb/usb_storage.h"
#include "usb/usb_host.h"
#include "usb/usb_phy.h"
#include "asm/uart.h"

struct usb_request_block {
    void *ptr;
    u32 len;
    u32 target_ep;
    u32 msg;
};
static struct usb_request_block urb;

static volatile u8 usb_host_sem = 0;
int usb_sem_init(struct usb_host_device  *host_dev)
{

    usb_host_sem = 0;
    return 0;
}
int usb_sem_pend(struct usb_host_device  *host_dev, u32 timeout)
{
    while (usb_host_sem == 0) {
        /* if (time_after(jiffies, timeout)) { */
        /* return OS_TIMEOUT; */
        /* } */
    }
    usb_host_sem = 0;
    return 0;
}
int usb_sem_post(struct usb_host_device  *host_dev)
{
    usb_host_sem = 1;
    return 0;
}

static void usb_bulk_tx_isr(struct usb_host_device *host_dev, u32 ep)
{
    usb_dev usb_id = 0;
    int l  = min(urb.len, 0x40);
    l = usb_h_ep_write_async(usb_id, ep, 0x40, urb.target_ep, urb.ptr, l, USB_ENDPOINT_XFER_BULK, 0);

    if (l > 0) {
        urb.len -= l;
        urb.ptr += l;
        urb.msg = 0;
    } else {
        urb.msg = l;
        urb.len = 0;
    }

    if (urb.len == 0) {
        if (urb.msg || l == 0) {
            usb_sem_post(host_dev);
        }
    }
}
s32 usb_bulk_only_send(struct usb_host_device *host_dev, u8 host_ep, u16 txmaxp, u8 target_ep, const u8 *pBuf, u32 len)
{
    const usb_dev usb_id = 0;//host_device2id(host_dev);

    urb.target_ep = target_ep;
    urb.msg = -1;
    urb.len = len - min(len, 0x40);
    urb.ptr = (u8 *)pBuf + min(len, 0x40);
#if 1//改成同步方式
    return usb_h_ep_write(usb_id, host_ep, txmaxp, target_ep, pBuf, len, USB_ENDPOINT_XFER_BULK);
#else

    usb_h_set_ep_isr(host_dev, host_ep, usb_bulk_tx_isr, host_dev);
    usb_set_intr_txe(usb_id, host_ep);

    int ret = usb_h_ep_write_async(usb_id, host_ep, txmaxp, target_ep, pBuf, min(len, 0x40), USB_ENDPOINT_XFER_BULK, 1);
    if (ret < 0) {
        goto __exit;
    }
    ret = usb_sem_pend(host_dev, 250);

    usb_clr_intr_txe(usb_id, host_ep);
    usb_h_set_ep_isr(host_dev, host_ep, NULL, host_dev);

    if (ret) {
        usb_printf("ret %d", ret);
        goto __exit;
    }
    /* g_usb_printf("%s() %d %d", __func__, urb.len, urb.msg); */
    return urb.msg ? urb.msg : len;
__exit:
    if (ret == -1) {
        /* ret = usb_clear_feature(host_dev, target_ep); */
        if (ret == 0) {
            return -1;
        }
    }
    return 0;
#endif
}
static void usb_bulk_rx_isr(struct usb_host_device *host_dev, u32 ep)
{
    int l  = min(urb.len, 0x40);
    l = usb_h_ep_read_async(0, ep, urb.target_ep, urb.ptr, l, USB_ENDPOINT_XFER_BULK, 0);
    /* g_usb_printf("%s() %d %d", __func__, l, urb.len); */
    if (l > 0) {
        urb.len -= l;
        if (urb.ptr) {
            urb.ptr += l;
        }
        urb.msg = 0;
    } else {
        urb.msg = l;
        urb.len = 0;
    }

    if (urb.len == 0) {
        usb_sem_post(host_dev);
    } else {
        usb_h_ep_read_async(0, ep,  urb.target_ep, NULL, 0, USB_ENDPOINT_XFER_BULK, 1);
    }
}
s32 usb_bulk_only_receive(struct usb_host_device *host_dev, u8 host_ep, u16 rxmaxp, u8 target_ep, u8 *pBuf, u32 len)
{

    const usb_dev usb_id = 0;//host_device2id(host_dev);
    urb.ptr = pBuf;
    urb.len = len;
    urb.target_ep = target_ep;
    urb.msg = -1;

#if 1//改成同步方式
    return usb_h_ep_read(usb_id, host_ep, rxmaxp, target_ep, pBuf, len, USB_ENDPOINT_XFER_BULK);
#else
    usb_h_set_ep_isr(host_dev, host_ep | USB_DIR_IN, usb_bulk_rx_isr, host_dev);
    usb_set_intr_rxe(0, host_ep);

    int ret = usb_h_ep_read_async(0, host_ep, target_ep, NULL, 0, USB_ENDPOINT_XFER_BULK, 1);
    if (ret < 0) {
        return ret;
    }
    ret = usb_sem_pend(host_dev, 250);
    usb_clr_intr_rxe(0, host_ep);
    usb_h_set_ep_isr(host_dev, host_ep | USB_DIR_IN, NULL, host_dev);
    if (ret) {
        return -1;
    }
    return urb.msg ? urb.msg : len;
#endif
}

