#ifndef __USB_BULK_TRANSFER_H__
#define __USB_BULK_TRANSFER_H__
#include "common.h"
#include "usb_host.h"


// s32 usb_bulk_only_receive_async(struct device *device, u8 host_ep, u16 rxmaxp, u8 target_ep, u8 *pBuf, u32 len);
// s32 usb_bulk_only_send_async(struct device *device, u8 host_ep, u16 txmaxp, u8 target_ep, const u8 *pBuf, u32 len);
s32 usb_bulk_only_send(struct usb_host_device *host_dev, u8 host_ep, u16 txmaxp, u8 target_ep, const u8 *pBuf, u32 len);
s32 usb_bulk_only_receive(struct usb_host_device *host_dev, u8 host_ep, u16 rxmaxp, u8 target_ep, u8 *pBuf, u32 len);

#endif
