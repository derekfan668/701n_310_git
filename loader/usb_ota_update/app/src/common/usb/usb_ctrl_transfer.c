/**
 * @file usb_ctrl_transfer.c
 * @brief usb 控制传输接口
 * @author chenrixin@zh-jieli.com
 * @version 1.00
 * @date 2017-02-09
 */

#include "usb/usb_host.h"
#include "usb/usb_ctrl_transfer.h"
#include "asm/gpio.h"
#include "usb/scsi.h"
#include "asm/uart.h"
#include "asm/usb.h"

#if 1//TCFG_UDISK_ENABLE || TCFG_ADB_ENABLE ||TCFG_AOA_ENABLE

/**
 * @brief usb_ctlXfer
 *
 * @param host_dev
 * @param urb
 *
 * @return
 */
static int usb_ctlXfer(struct usb_host_device *host_dev, struct ctlXfer *urb)
{
    u32 ret = 0;
    u8 reg = 0;
    u32 data_len;
    usb_dev usb_id = 0;//host_device2id(host_dev);
    u8 devnum = host_dev->private_data.devnum;
    u32 max_packet_size = host_dev->private_data.ep0_max_packet_size;

    usb_write_faddr(usb_id, devnum);

    switch (urb->stage) {
    case USB_PID_SETUP :
        usb_write_ep0(usb_id, (u8 *)&urb->setup, 8);

        reg = CSR0H_SetupPkt | CSR0H_TxPktRdy;
        break;
    case USB_PID_IN :
        if (urb->setup.wLength) {
            reg = CSR0H_ReqPkt;
        } else {
            reg = CSR0H_StatusPkt | CSR0H_ReqPkt;
        }
        break;
    case USB_PID_OUT:
        if (urb->setup.wLength) {
            data_len = min(urb->setup.wLength, max_packet_size);
            reg = CSR0H_TxPktRdy;
            usb_write_ep0(usb_id, urb->buffer, data_len);
            urb->setup.wLength -= data_len;
            urb->buffer += data_len;
        } else {
            reg = CSR0H_StatusPkt | CSR0H_TxPktRdy;
        }
        break;
    default :
        break;
    }

    usb_write_csr0(usb_id, reg);

    u32 st = 0;
    /* u32 ot = get_jiffies() + 500; */
    while (1) {
        /* if (usb_host_timeout(ot)) { */
        usb_mdelay(10);
        /* usb_printf("time out %x\n", reg); */
        /* ret = -1; */
        /* goto __exit; */
        /* } */
        if (usb_h_dev_status(usb_id)) {
            st ++;
        } else {
            st = 0;
        }
        if (((usb_read_devctl(usb_id) & BIT(2)) == 0) || (st > 1000)) {
            usb_printf("usb%d_offline\n", usb_id);
            ret = -1;
            goto __exit;
        }

        reg = usb_read_csr0(usb_id);
        if (reg & CSR0H_RxStall) {
            usb_printf(" rxStall CSR0:0x%x", reg);
            ret = -1;
            goto __exit;
        }
        if (reg & CSR0H_Error) {
            usb_printf(" Error CSR0:0x%x", reg);
            usb_write_csr0(usb_id, 0);
            ret = -1;
            goto __exit;
        }
        if (USB_PID_IN == urb->stage) {

            if (reg & CSR0H_RxPktRdy) {
                data_len = usb_read_count0(usb_id);
                data_len = min(data_len, urb->setup.wLength);
                usb_read_ep0(usb_id, urb->buffer, data_len);;
                urb->buffer += data_len;
                urb->setup.wLength -= data_len;
                if (data_len < max_packet_size) {
                    urb->setup.wLength = 0;
                }
                if (urb->setup.wLength) {
                    usb_write_csr0(usb_id, CSR0H_ReqPkt);
                } else {
                    usb_write_csr0(usb_id, 0);
                    break;
                }
            }
        } else {
            if (!(reg & CSR0H_TxPktRdy)) {
                break;
            }
        }
    }
__exit:
    /* usb_clr_intr_txe(usb_id, 0); */
    return ret;
}
/**
 * @brief usb_control_transfers
 *
 * @param struct host_dev
 * @param urb
 *
 * @return
 */
static int usb_control_transfers(struct usb_host_device *host_dev, struct ctlXfer *urb)
{

    int res;
    /*SETUP*/

    urb->stage = USB_PID_SETUP;		//SETUP transaction

    res = usb_ctlXfer(host_dev, urb);

    if (res) {
        return res;
    }

    /*IN or OUT*/
    urb->stage = USB_PID_IN;


    while (urb->setup.wLength) {
        if (urb->setup.bRequestType & USB_DIR_IN) {	//Request Direction
            urb->stage = USB_PID_IN;	//IN transaction

            res = usb_ctlXfer(host_dev, urb);

            if (res) {
                return res;
            }

            urb->stage = USB_PID_OUT;
        } else {
            urb->stage = USB_PID_OUT;	//OUT transaction

            res = usb_ctlXfer(host_dev, urb);

            if (res) {
                return res;
            }

            urb->stage = USB_PID_IN;
        }
    }

    res = usb_ctlXfer(host_dev, urb);

    if (res) {
        return res;
    }

    return 0;
}

/**
 * @brief usb_control_msg
 *
 * @param host_dev
 * @param request
 * @param requesttype
 * @param value
 * @param index
 * @param data
 * @param size
 *
 * @return
 */
static int usb_control_msg(struct usb_host_device *host_dev,
                           u8 request, u8 requesttype,
                           u16 value, u16 index,
                           void *data, u16 size)
{
    struct ctlXfer urb;
    urb.setup.bRequestType = requesttype;
    urb.setup.bRequest = request;
    urb.setup.wValue = cpu_to_le16(value);
    urb.setup.wIndex = cpu_to_le16(index);
    urb.setup.wLength = cpu_to_le16(size);
    urb.buffer = data;

    return usb_control_transfers(host_dev, &urb);

}

/**
 * @brief usb_clear_feature
 *
 * @param host_dev
 * @param ep
 *
 * @return
 */
int usb_clear_feature(struct usb_host_device *host_dev, u32 ep)
{
    return usb_control_msg(host_dev,  USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT, 0, ep, NULL, 0);
}

int set_address(struct usb_host_device *host_dev, u8 devnum)
{
    return usb_control_msg(host_dev, USB_REQ_SET_ADDRESS, 0,  devnum, 0, NULL, 0);
}

int usb_get_device_descriptor(struct usb_host_device *host_dev, struct usb_device_descriptor *desc)
{
    return usb_control_msg(host_dev,
                           USB_REQ_GET_DESCRIPTOR,
                           USB_DIR_IN,
                           (USB_DT_DEVICE << 8),
                           0,
                           desc,
                           USB_DT_DEVICE_SIZE
                          );
}

int usb_get_string_descriptor(struct usb_host_device *host_dev, struct usb_device_descriptor *desc)
{
    int ret = 0;
#if 0
    struct usb_private_data *private_data = host_dev->private_data ;
    /**********get string language*********/
    u8 buf[16];
    memset(buf, 0x0, sizeof(buf));
    ret = usb_control_msg(host_dev,
                          USB_REQ_GET_DESCRIPTOR,
                          USB_DIR_IN,
                          (USB_DT_STRING << 8),
                          0,
                          desc,
                          sizeof(buf)
                         );
    if (ret) {
        return ret;
    }
    memcpy(&private_data->language, buf + 2, sizeof(private_data->language));

    /**********get manufacturer string**********/
    memset(private_data->manufacturer, 0x0, sizeof(private_data->manufacturer));
    if (desc->iManufacturer) {
        ret = usb_control_msg(host_dev,
                              USB_REQ_GET_DESCRIPTOR,
                              USB_DIR_IN,
                              (USB_DT_STRING << 8) | desc->iManufacturer,
                              0,
                              private_data->manufacturer,
                              sizeof(private_data->manufacturer)
                             );
        if (ret) {
            return ret;
        }
    }
    /**********get product string**********/
    memset(private_data->product, 0x0, sizeof(private_data->product));
    if (desc->iProduct) {
        ret = usb_control_msg(host_dev,
                              USB_REQ_GET_DESCRIPTOR,
                              USB_DIR_IN,
                              (USB_DT_STRING << 8) | desc->iProduct,
                              0,
                              private_data->product,
                              sizeof(private_data->product)
                             );
        if (ret) {
            return ret;
        }
    }
#endif
    return ret;
}

int set_configuration(struct usb_host_device *host_dev)
{
    return  usb_control_msg(host_dev, USB_REQ_SET_CONFIGURATION, 0, 1, 0, NULL, 0);
}
int get_config_descriptor(struct usb_host_device *host_dev, void *cfg_desc, u32 len)
{
    return usb_control_msg(host_dev,
                           USB_REQ_GET_DESCRIPTOR,
                           USB_DIR_IN,
                           (USB_DT_CONFIG << 8),
                           0,
                           cfg_desc,
                           len);
}

int get_msd_max_lun(struct usb_host_device *host_dev, void *lun)
{
    return usb_control_msg(host_dev,
                           USB_MSD_MAX_LUN,
                           USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                           0,
                           0,
                           lun,
                           1);
}
int set_msd_reset(struct usb_host_device *host_dev)
{
    return usb_control_msg(host_dev,
                           0xff,
                           USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                           0,
                           0,
                           NULL,
                           0);
}

int get_device_status(struct usb_host_device *host_dev)
{
    u16 status;
    return usb_control_msg(host_dev, USB_REQ_GET_STATUS, USB_DIR_IN, 0, 0, (u8 *)&status, 2);
}


#endif
