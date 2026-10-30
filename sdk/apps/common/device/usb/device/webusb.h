#ifndef  __WEBUSB_H__
#define  __WEBUSB_H__

#include "usb/usb_config.h"
#include "usb/device/usb_stack.h"

u32 webusb_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num);
u32 webusb_register(usb_dev usb_id);
void webusb_release(usb_dev usb_id);

void webusb_set_rx_hook(void *priv, void (*rx_hook)(void *priv, u8 *buf, u32 len));
u32 webusb_tx_data(const usb_dev usb_id, const u8 *buffer, u32 len);

#endif  /*WEBUSB_H*/

