
/** * @file usb_storage.c
 * @brief
 * @author chenrixin@zh-jieli.com
 * @version 1.00
 * @date 2017-02-09
 */

#include "usb/usb_host.h"
#include "usb/usb_ctrl_transfer.h"
#include "usb/usb_bulk_transfer.h"
#include "usb/usb_storage.h"
#include "asm/uart.h"

#if 1
//#ifdef UDISK0_LOADER
static struct mass_storage mass_stor ;//SEC(.usb_h_udisk);
static struct udisk_end_desc udisk_ep ;//SEC(.usb_h_udisk);

/**
 * @brief usb_msd_parser
 *
 * @param device
 * @param pBuf
 *
 * @return
 */
static struct usb_host_device *host_msd_dev = NULL;
int usb_msd_parser(struct usb_host_device *host_dev, u8 interface_num, const u8 *pBuf)
{
    struct usb_interface_descriptor *interface = (struct usb_interface_descriptor *)pBuf;
    pBuf += sizeof(struct usb_interface_descriptor);
    int len = 0;
    const usb_dev usb_id = 0;//host_device2id(host_dev);

    host_msd_dev = host_dev;

    struct mass_storage *disk = &mass_stor;

    for (int endnum = 0; endnum < interface->bNumEndpoints; endnum++) {
        struct usb_endpoint_descriptor *end_desc = (struct usb_endpoint_descriptor *)pBuf;

        if (end_desc->bDescriptorType != USB_DT_ENDPOINT ||
            end_desc->bLength != USB_DT_ENDPOINT_SIZE) {
            usb_printf("ep bDescriptorType = %d bLength = %d", end_desc->bDescriptorType, end_desc->bLength);
            return -USB_DT_ENDPOINT;
        }

        len += USB_DT_ENDPOINT_SIZE;
        pBuf += USB_DT_ENDPOINT_SIZE;

        if ((end_desc->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
            if (end_desc->bEndpointAddress & USB_DIR_IN) {
                udisk_ep.host_epin = usb_get_ep_num(usb_id, USB_DIR_IN, USB_ENDPOINT_XFER_BULK);
                udisk_ep.target_epin = end_desc->bEndpointAddress & 0x0f;
#if HUSB_MODE
                udisk_ep.rxmaxp = end_desc->wMaxPacketSize;
#endif
                usb_printf("D(%d)->H(%d)\n", udisk_ep.target_epin, udisk_ep.host_epin);
            } else {
                udisk_ep.host_epout = usb_get_ep_num(usb_id, USB_DIR_OUT, USB_ENDPOINT_XFER_BULK);
                udisk_ep.target_epout = end_desc->bEndpointAddress & 0x0f;
#if HUSB_MODE
                udisk_ep.txmaxp = end_desc->wMaxPacketSize;
#endif
                usb_printf("H(%d)->D(%d)\n",  udisk_ep.host_epout, udisk_ep.target_epout);
            }
        }
    }
    u8 *ep_buffer = usb_h_get_ep_buffer(usb_id, udisk_ep.host_epin | USB_DIR_IN);
    usb_h_ep_config(usb_id, udisk_ep.host_epin | USB_DIR_IN, USB_ENDPOINT_XFER_BULK, 0, 0, ep_buffer, 64);


    ep_buffer = usb_h_get_ep_buffer(usb_id, udisk_ep.host_epout | USB_DIR_OUT);
    usb_h_ep_config(usb_id, udisk_ep.host_epout | USB_DIR_OUT, USB_ENDPOINT_XFER_BULK, 0, 0, ep_buffer, 64);
    disk->dev_status = DEV_READY;
    return len;
}

static void usb_init_cbw(struct usb_host_device *device, u32 dir, u32 opcode, u32 length)
{
    struct mass_storage *disk = &mass_stor;

    memset(&disk->cbw, 0x00, sizeof(struct usb_scsi_cbw));
    u32 curlun = disk->cur_available_lun;
    disk->cbw.dCBWSignature = CBW_SIGNATURE;
    disk->cbw.dCBWTag ++ ;//rand32();
    disk->cbw.bCBWLUN = curlun;
    disk->cbw.lun = curlun << 5;
    disk->cbw.bmCBWFlags = dir;
    disk->cbw.operationCode = opcode;
    disk->cbw.dCBWDataTransferLength = cpu_to_le32(length);
}
/**
 * @brief usb_stor_inquiry
 *
 * @param device
 *
 * @return
 */
static int usb_stor_inquiry(struct usb_host_device *host_dev)
{
    struct mass_storage *disk = &mass_stor;

    const u32 txmaxp = 0x40;
    const u32 rxmaxp = 0x40;

    int ret = 0;
    struct inquiry_data inquiry;

    usb_printf("usb_stor_inquiry disk =%x\n", (u32)disk);

    u32 total_lun = 1;//disk->lun;

    for (int i = 0; i <= total_lun; i ++) {
        usb_init_cbw(host_dev, USB_DIR_IN, INQUIRY, sizeof(struct inquiry_data));
        disk->cbw.bCBWLength = 0x06;
        disk->cbw.lba[2] = sizeof(struct inquiry_data);

        //cbw
        ret = usb_bulk_only_send(host_dev,
                                 udisk_ep.host_epout,
                                 txmaxp,
                                 udisk_ep.target_epout,
                                 (u8 *)&disk->cbw,
                                 sizeof(struct usb_scsi_cbw));

        if (ret < 0) {
            usb_printf("%s:%d\n", __func__, __LINE__);
            goto __exit;
        }
        //data
        ret = usb_bulk_only_receive(host_dev,
                                    udisk_ep.host_epin,
                                    rxmaxp,
                                    udisk_ep.target_epin,
                                    (u8 *)&inquiry,
                                    sizeof(struct inquiry_data));

        /* if (ret < 0) { */
        /* usb_printf("%s:%d\n", __func__, __LINE__); */
        /* goto __exit; */
        /* } else  */
        if (ret != sizeof(struct inquiry_data)) {
            ret = -1;
            usb_printf("%s:%d\n", __func__, __LINE__);
            goto __exit;
        }

        /* usb_printf_buf(&inquiry, sizeof(struct INQUIRY_DATA)); */
        //csw
        ret = usb_bulk_only_receive(host_dev,
                                    udisk_ep.host_epin,
                                    rxmaxp,
                                    udisk_ep.target_epin,
                                    (u8 *)&disk->csw,
                                    sizeof(struct usb_scsi_csw));

        /* if (ret < 0) { */
        /* usb_printf("%s:%d\n", __func__, __LINE__); */
        /* goto __exit; */
        /* } else  */
        if (ret != sizeof(struct usb_scsi_csw)) {
            ret = -1;
            usb_printf("%s:%d\n", __func__, __LINE__);
            goto __exit;
        }
        if (inquiry.PeripheralDeviceType) {
            continue;
        } else {
            break;
        }
    }

    return 0;
__exit:
    return ret;
}
/**
 * @brief usb_stor_test_unit_ready
 *
 * @param device
 *
 * @return
 */
static int usb_stor_test_unit_ready(struct usb_host_device *host_dev)
{
    int ret = 0;

    struct mass_storage *disk = &mass_stor;
    const usb_dev usb_id = 0;//host_device2id(host_dev);

    const u32 txmaxp = 0x40;
    const u32 rxmaxp = 0x40;

    usb_init_cbw(host_dev, 0, TEST_UNIT_READY, 0);
    disk->cbw.bCBWLength = 6;
    //cbw
    ret = usb_bulk_only_send(host_dev,
                             udisk_ep.host_epout,
                             txmaxp,
                             udisk_ep.target_epout,
                             (u8 *)&disk->cbw,
                             sizeof(struct usb_scsi_cbw));

    if (ret < 0) {
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    //csw
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                (u8 *)&disk->csw,
                                sizeof(struct usb_scsi_csw));

    /* if (ret < 0) { */
    /* usb_printf("%s:%d\n", __func__, __LINE__); */
    /* goto __exit; */
    /* } else */
    if (ret != sizeof(struct usb_scsi_csw)) {
        ret = -1;
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    return 0;
__exit:
    usb_printf("%s---%d\n", __func__, __LINE__);
    return ret;
}
static int usb_stor_request_sense(struct usb_host_device *host_dev)
{
    struct mass_storage *disk = &mass_stor;
    const usb_dev usb_id = 0;//host_device2id(host_dev);
    int ret = 0;

    const u32 txmaxp = 0x40;
    const u32 rxmaxp = 0x40;

    usb_init_cbw(host_dev, USB_DIR_IN, REQUEST_SENSE, sizeof(struct request_sense_data));
    disk->cbw.bCBWLength = 0xC;
    disk->cbw.lba[2] = sizeof(struct request_sense_data);
    //cbw
    ret = usb_bulk_only_send(host_dev,
                             udisk_ep.host_epout,
                             txmaxp,
                             udisk_ep.target_epout,
                             (u8 *)&disk->cbw,
                             sizeof(struct usb_scsi_cbw));

    if (ret < 0) {
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    //data
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                (u8 *)&disk->sense,
                                sizeof(struct request_sense_data));

    /* if (ret < 0) { */
    /* usb_printf("%s:%d\n", __func__, __LINE__); */
    /* goto __exit; */
    /* } else  */
    if (ret != sizeof(struct request_sense_data)) {
        ret = -1;
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    /* usb_printf_buf(&private_data->dev.disk->sense, sizeof(struct REQUEST_SENSE_DATA)); */
    //csw
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                (u8 *)&disk->csw,
                                sizeof(struct usb_scsi_csw));

    /* if (ret < 0) { */
    /* usb_printf("%s:%d\n", __func__, __LINE__); */
    /* goto __exit; */
    /* } else */
    if (ret != sizeof(struct usb_scsi_csw)) {
        ret = -1;
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    return 0;
__exit:
    usb_printf("%s---%d\n", __func__, __LINE__);
    return ret;
}
/**
 * @brief usb_stor_read_capacity
 *
 * @param device
 *
 * @return
 */
static int usb_stor_read_capacity(struct usb_host_device *host_dev)
{
    struct mass_storage *disk = &mass_stor;
    const usb_dev usb_id = 0;//host_device2id(host_dev);
    int ret = 0;

    const u32 txmaxp = 0x40;
    const u32 rxmaxp = 0x40;

    u32 curlun = 0;

    usb_init_cbw(host_dev, USB_DIR_IN, READ_CAPACITY, 8);
    disk->cbw.bCBWLength = 0xA;
    disk->cbw.bmCBWFlags = USB_DIR_IN;
    disk->cbw.operationCode = READ_CAPACITY;
    disk->capacity[curlun].block_size = 0;
    disk->capacity[curlun].block_num = 0;

    //cbw
    ret = usb_bulk_only_send(host_dev,
                             udisk_ep.host_epout,
                             txmaxp,
                             udisk_ep.target_epout,
                             (u8 *)&disk->cbw,
                             sizeof(struct usb_scsi_cbw));


    if (ret < 0) {
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    //data
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                (u8 *)&disk->capacity[curlun],
                                sizeof(struct read_capacity_data));

    /* if (ret < 0) { */
    /* usb_printf("%s:%d\n", __func__, __LINE__); */
    /* goto __exit; */
    /* } else  */
    if (ret != sizeof(struct read_capacity_data)) {
        ret = -1;
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    disk->capacity[curlun].block_size = be32_to_cpu(disk->capacity[curlun].block_size);
    disk->capacity[curlun].block_num = be32_to_cpu(disk->capacity[curlun].block_num) + 1;
    usb_printf("block_size=%d\n", disk->capacity[curlun].block_size);
    usb_printf("block_num=%d\n", disk->capacity[curlun].block_num);
    //csw
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                (u8 *)&disk->csw,
                                sizeof(struct usb_scsi_csw));

    if (ret < 0) {
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    } else if (ret != sizeof(struct usb_scsi_csw)) {
        ret = -1;
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    return 0;
__exit:
    return ret;
}
/**
 * @brief usb_stor_init
 *
 * @param device
 *
 * @return
 */
int usb_stor_init()
{
    int ret = 0;
    struct usb_host_device *host_dev = host_msd_dev;

    const u32 txmaxp = 0x40;
    const u32 rxmaxp = 0x40;
    u32 state = 0;
    struct mass_storage *disk = &mass_stor;

    u8 lun;
    ret = get_msd_max_lun(host_dev, &lun);
    if (ret != 0) {
        usb_printf("ret = %d\n", ret);
        return ret;
    }
    disk->cur_available_lun = 0;
    int retry = 5;
    while (retry) {
        retry --;
        if (ret == -1) {
            usb_printf("disk offline\n");
            break;
        }
        switch (state) {
        case 0:
            usb_printf("--- inquery ---\n");
            ret = usb_stor_inquiry(host_dev);
            if (ret < 0) {
                continue;
                if (retry == 3) {
                    ret = -1;
                }
            } else {
                state++;
            }
            break;

        case 1:
            usb_printf("--- test unit ready ---\n");
            ret = usb_stor_test_unit_ready(host_dev);
            if (ret < 0) {
                continue;
            } else if (disk->csw.bCSWStatus) {
                ret = usb_stor_request_sense(host_dev);
                if ((disk->sense.SenseKey & 0x0f) !=  NO_SENSE) {
                    if (disk->sense.SenseKey == NOT_READY) {
                        if (disk->cur_available_lun++ == lun) {
                            disk->cur_available_lun = 0;
                        }
                    }
                }
            } else {
                state++;
            }
            break;

        case 2:
            usb_printf("--- read capacity ---\n");
            ret = usb_stor_read_capacity(host_dev);
            if (ret < 0) {
                continue;
            } else if (disk->csw.bCSWStatus) {
                ret = usb_stor_request_sense(host_dev);
                if ((disk->sense.SenseKey & 0x0f) !=  NO_SENSE) {
                    continue;
                }
            } else {
                usb_printf(">>> udisk mount succ\n");
                state = 0xff;
            }
            break;

        default:
            retry = 0;
            break;
        }
    }
    state = 0;

    /* if (ret != 0) { */
    /* disk->dev_status = DEV_IDLE; */
    /* usb_printf("ret = %d\n", ret); */
    /* } else { */
    /* } */
    return ret;
}
static int _usb_stro_read_cbw_request(struct usb_host_device *host_dev, u32 num_lba, u32 lba)
{
    struct mass_storage *disk = &mass_stor;

    const u32 txmaxp = 0x40;
    u32 rx_len = num_lba * 512;  //filesystem uses the fixed BLOCK_SIZE
    u32 cur_lba = lba;
    int ret = 0;

    disk->dev_status = DEV_READ;
    usb_init_cbw(host_dev, USB_DIR_IN, READ_10, rx_len);
    disk->cbw.bCBWLength = 0xA;

    cur_lba = cpu_to_be32(cur_lba);
    memcpy(disk->cbw.lba, &cur_lba, sizeof(cur_lba));

    disk->cbw.LengthH = HIBYTE(num_lba);
    disk->cbw.LengthL = LOBYTE(num_lba);

    //cbw
    ret = usb_bulk_only_send(host_dev,
                             udisk_ep.host_epout,
                             txmaxp,
                             udisk_ep.target_epout,
                             (u8 *)&disk->cbw,
                             sizeof(struct usb_scsi_cbw));

    if (ret < 0) {
        /* if (disk->dev_status != DEV_CLOSE) { */
        /* disk->dev_status = DEV_OPEN; */
        /* } */
        usb_printf("%s---%d\n", __func__, __LINE__);
        return ret;
    }
    return num_lba;
}
/**
 * @brief usb_stor_read 从U盘的lba扇区读取num_lba个扇区
 *
 * @param device 设备句柄
 * @param pBuf 读buffer缓冲区，芯片所有memory都可以
 * @param lba 需要读取的扇区地址
 * @param num_lba 需要读取的扇区数量
 *
 * @return 负数表示失败
 * 			大于0的表示读取的字节数(Byte)
 */
int usb_stor_read(void *pBuf, u32 num_lba, u32 lba)
{
    usb_printf("%s,%d\n", __func__, lba);
    struct mass_storage *disk = &mass_stor;
    /* const usb_dev usb_id = host_device2id(host_dev); */
    struct usb_host_device *host_dev = host_id2device(0);

    const u32 txmaxp = 0x40;
    const u32 rxmaxp = 0x40;

    int ret;
    u32 rx_len = num_lba * disk->capacity[0].block_size;


    if (lba + num_lba >= disk->capacity[0].block_num) {
        return -1;
    }
    ret = _usb_stro_read_cbw_request(host_dev, num_lba, lba);
    if (ret < 0) {
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    //data
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                pBuf,
                                rx_len);
    if (ret < 0) {
        usb_printf("%s:%d, %d, %d\n", __func__, __LINE__, ret, rx_len);
        goto __exit;
    }

    //csw
    ret = usb_bulk_only_receive(host_dev,
                                udisk_ep.host_epin,
                                rxmaxp,
                                udisk_ep.target_epin,
                                (u8 *)&disk->csw,
                                sizeof(struct usb_scsi_csw));

    /* if (ret < 0) { */
    /* usb_printf("%s:%d\n", __func__, __LINE__); */
    /* goto __exit; */
    /* } else */
    if (ret != sizeof(struct usb_scsi_csw)) {
        ret = -1;
        usb_printf("%s:%d\n", __func__, __LINE__);
        goto __exit;
    }

    disk->dev_status = DEV_OPEN;
    return num_lba;

__exit:
    if (disk->dev_status != DEV_CLOSE) {
        disk->dev_status = DEV_OPEN;
    }
    usb_printf("%s---%d\n", __func__, __LINE__);
    return ret;
}

u8 usb_read_udisk(u8 *buf, u32 sector)
{
    int err = usb_stor_read(buf, 1, sector);
    return err;
}
#endif

