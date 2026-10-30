
#ifndef __USB_STORAGE_H__
#define __USB_STORAGE_H__

/* #include "system/task.h" */
#include "scsi.h"
#include "usb_bulk_transfer.h"
#include "usb_host.h"

//设备状态：
typedef enum usb_sta {
    DEV_IDLE = 0,
    DEV_INIT,
    DEV_OPEN,
    DEV_READ,
    DEV_WRITE,
    DEV_CLOSE,
    DEV_SUSPEND,
    DEV_READY,
} USB_STA ;

struct udisk_end_desc {
    u8 host_epout;
    u8 target_epout;
    u8 host_epin;
    u8 target_epin;
};

struct mass_storage {
    // OS_MUTEX mutex;

    struct usb_scsi_cbw cbw;
    struct usb_scsi_csw csw;
    struct request_sense_data sense;

    struct read_capacity_data capacity[1];

    u8 dev_status;
    volatile u8 cur_available_lun;

};


int usb_msd_parser(struct usb_host_device *host_dev, u8 interface_num, const u8 *pBuf);
int usb_stor_init();
int usb_stor_read(void *pBuf, u32 num_lba, u32 lba);
u8 usb_read_udisk(u8 *buf, u32 sector);

#endif
