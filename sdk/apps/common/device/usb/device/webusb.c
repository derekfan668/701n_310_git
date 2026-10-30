#ifdef SUPPORT_MS_EXTENSIONS
#pragma bss_seg(".usb.data.bss")
#pragma data_seg(".usb.data")
#pragma code_seg(".usb.text")
#pragma const_seg(".usb.text.const")
#pragma str_literal_override(".usb.text.const")
#endif

#include "os/os_api.h"
#include "usb/device/webusb.h"
#include "usb_config.h"
#include "app_config.h"

#define LOG_TAG_CONST       USB
#define LOG_TAG             "[USB]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#if TCFG_USB_SLAVE_ENABLE && TCFG_USB_WEBUSB_ENABLE

#define MS_OS_20_SET_HEADER_DESCRIPTOR 0x00
#define MS_OS_20_SUBSET_HEADER_CONFIGURATION 0x01
#define MS_OS_20_SUBSET_HEADER_FUNCTION 0x02
#define MS_OS_20_FEATURE_COMPATIBLE_ID 0x03
#define MS_OS_20_FEATURE_REG_PROPERTY 0x04
#define MS_OS_20_DESCRIPTOR_LENGTH 0x1e
#define MS_OS20_OFFSET_BFIRSTINTERFACE   (0x0A + 0x08 + 0x04)

#define     WEBUSB_REQ_URL          (1)
#define     WEBUSB_REQ_MS_DESC      (2)

#define     WEBUSB_iInterface   0x11
static u8 webusb_interface_num;

typedef void (*webusb_rx_handle_t)(void *hdl, u8 *buffer, u32 len);
struct webusb_handle_t {
    u8 *ep_out_dmabuffer;
    u8 *ep_in_dmabuffer;
    void (*wakeup_handle)(struct usb_device_t *usb_device);
    void *priv_hdl;
    webusb_rx_handle_t rx_hook;
};

static struct webusb_handle_t *webusb_handle = NULL;
static void webusb_epout_isr(struct usb_device_t *usb_device, u32 ep);
static void webusb_epin_isr(struct usb_device_t *usb_device, u32 ep);

static const u8 sWebUSBDescriptor[] = {
    0x09,       // bLength
    0x04,       // bDescriptorType = Interface
    0x00,       // bInterfaceNumber = 0
    0x00,       // bAlternateSetting
    0x02,       // bNumEndpoints = 2
    0xFF,       // bInterfaceClass = Vendor Specific (0xFF)
    0x00,       // bInterfaceSubClass
    0x00,       // bInterfaceProtocol
    WEBUSB_iInterface,       // iInterface

    //EndpointDescriptor:
    USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType, Type
    USB_DIR_IN | WEBUSB_EP_IN,     // bEndpointAddress
    USB_ENDPOINT_XFER_BULK,      // Interrupt
    LOBYTE(MAXP_SIZE_WEBUSBIN), HIBYTE(MAXP_SIZE_WEBUSBIN),// Maximum packet size
    0,        // bInterval, for high speed 2^(n-1) * 125us, for full/low speed n * 1ms

    //Endpoint Descriptor:
    USB_DT_ENDPOINT_SIZE,       // bLength
    USB_DT_ENDPOINT,            // bDescriptorType, Type
    WEBUSB_EP_OUT,   // bEndpointAddress
    USB_ENDPOINT_XFER_BULK,      // Interrupt
    LOBYTE(MAXP_SIZE_WEBUSBOUT), HIBYTE(MAXP_SIZE_WEBUSBOUT),// Maximum packet size
    0,        // bInterval, for high speed 2^(n-1) * 125us, for full/low speed n * 1ms
};
/* --------------------------------------------------------------------
 * BOS Descriptor with WebUSB + Microsoft OS 2.0
 * ------------------------------------------------------------------ */

static const uint8_t bos_descriptor[] = {
    // BOS Descriptor
    0x05,       // bLength
    0x0F,       // bDescriptorType = BOS
    0x39, 0x00, // wTotalLength (57 bytes = 5 + 24 + 28)
    0x02,       // bNumDeviceCaps = 2

    // ---- WebUSB Platform Capability Descriptor ----
    0x18,       // bLength = 24
    0x10,       // bDescriptorType = Device Capability
    0x05,       // bDevCapabilityType = PLATFORM
    0x00,       // bReserved
    // PlatformCapabilityUUID (WebUSB UUID = 3408B638-09A9-47A0-8BFD-A0768815B665)
    0x38, 0xB6, 0x08, 0x34,
    0xA9, 0x09,
    0xA0, 0x47,
    0x8B, 0xFD,
    0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65,
    0x00, 0x01, // bcdVersion 1.0
    WEBUSB_REQ_URL,       // bVendorCode (for WebUSB requests)
    0x01,       // iLandingPage (string index for default URL)

    // ---- Microsoft OS 2.0 Platform Capability Descriptor ----
    0x1C,       // bLength = 28
    0x10,       // bDescriptorType = Device Capability
    0x05,       // bDevCapabilityType = PLATFORM
    0x00,       // bReserved
    // MS OS 2.0 Platform Capability UUID = D8DD60DF-4589-4C7C-9CD2-659D9E648A9F
    0xDF, 0x60, 0xDD, 0xD8,
    0x89, 0x45,
    0xC7, 0x4C,
    0x9C, 0xD2,
    0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F,
    0x00, 0x00, 0x03, 0x06, // dwWindowsVersion = 0x06030000 (Win 8.1+)
    MS_OS_20_DESCRIPTOR_LENGTH, 0x00,  //Descriptor set length
    WEBUSB_REQ_MS_DESC,       // bMS_VendorCode (for MS OS requests)
    0x00,                   // bAltEnumCode
};

/* --------------------------------------------------------------------
 * Microsoft OS 2.0 Descriptor Set
 * ------------------------------------------------------------------ */
static const uint8_t ms_os_20_descriptor[] = {
    // ---- Microsoft OS 2.0 Descriptor Set Header ----
    0x0A, 0x00,             // wLength = 10
    MS_OS_20_SET_HEADER_DESCRIPTOR, 0x00,             // wDescriptorType = SET_HEADER
    0x00, 0x00, 0x03, 0x06, // dwWindowsVersion = 0x06030000
    MS_OS_20_DESCRIPTOR_LENGTH, 0x00,             // Size, MS OS 2.0 descriptor set


    // ---- Compatible ID Feature Descriptor ----
    0x14, 0x00,  // wLength = 20
    MS_OS_20_FEATURE_COMPATIBLE_ID, 0x00,             // wDescriptorType = FEATURE_COMPATIBLE_ID
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, // Compatible ID = "WINUSB\0\0"
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Sub-Compatible ID (unused)

};

/* --------------------------------------------------------------------
 * WebUSB URL Descriptor (GET_URL response)
 * URL = https://doc.zh-jieli.com
 * ------------------------------------------------------------------ */
static const uint8_t webusb_url_descriptor[] = {
    3 + 16,     // bLength = (3 + 16 chars)
    0x03,       // bDescriptorType = URL
    0x01,       // bScheme = 1 (https://)
    'd', 'o', 'c', '.', 'z', 'h', '-', 'j', 'i', 'e', 'l', 'i', '.', 'c', 'o', 'm'
};

#define WEBUSB_REQUEST_GET_URL			0x02
#define MS_OS_20_REQUEST_DESCRIPTOR     0x07

static const uint8_t device_qualifier_descriptor[] = {
    0x0A,       // bLength
    0x06,       // bDescriptorType = DEVICE_QUALIFIER
    0x10, 0x02, // bcdUSB = 2.00
    0x00,       // bDeviceClass
    0x00,       // bDeviceSubClass
    0x00,       // bDeviceProtocol
    0x40,       // bMaxPacketSize0 = 64
    0x01,       // bNumConfigurations
    0x00        // bReserved
};

static const u8 iweb_usb_string[] = {
    44,         //该描述符的长度为44字节
    0x03,       //字符串描述符的类型编码为0x03
    0x77, 0x00, //w
    0x65, 0x00, //e
    0x62, 0x00, //b
    0x75, 0x00, //u
    0x73, 0x00, //s
    0x62, 0x00, //b
    0x5f, 0x00, //_
    0x69, 0x00, //i
    0x6e, 0x00, //n
    0x74, 0x00, //t
    0x65, 0x00, //e
    0x72, 0x00, //r
    0x66, 0x00, //f
    0x61, 0x00, //a
    0x63, 0x00, //c
    0x65, 0x00, //e
    0x5f, 0x00, //_
    0x6e, 0x00, //n
    0x61, 0x00, //a
    0x6d, 0x00, //m
    0x65, 0x00, //e
};

u32 webusb_setup_device_hook(struct usb_device_t *usb_device, struct usb_ctrlrequest *req)
{
    if (webusb_handle == NULL) {
        return 0;
    }
    u32 ret = 1;

    u32 h_wValue = HIBYTE(req->wValue);
    u32 l_wValue = LOBYTE(req->wValue);

    if (req->bRequest == USB_REQ_GET_DESCRIPTOR) {
        /* BOS Descriptor */
        if (h_wValue == USB_DT_BOS) {
            usb_set_data_payload(usb_device, req, bos_descriptor, sizeof(bos_descriptor));
        }
        /* Device Qualifier Descriptor */
        else if (h_wValue == USB_DT_DEVICE_QUALIFIER) {
            usb_set_data_payload(usb_device, req, device_qualifier_descriptor, sizeof(device_qualifier_descriptor));
        }
        /* WebUSB string descriptor */
        else if (h_wValue == USB_DT_STRING && l_wValue == WEBUSB_iInterface) {
            usb_set_data_payload(usb_device, req, iweb_usb_string, sizeof(iweb_usb_string));
        } else {
            ret = 0;
        }
    }
    /* Microsoft OS 2.0 Descriptor request */
    else if (req->bRequest == WEBUSB_REQ_MS_DESC && req->wIndex == MS_OS_20_REQUEST_DESCRIPTOR) {
        usb_set_data_payload(usb_device, req, ms_os_20_descriptor, sizeof(ms_os_20_descriptor));
    }
    /* WebUSB GET_URL request */
    else if (req->bRequest == WEBUSB_REQ_URL && req->wIndex == WEBUSB_REQUEST_GET_URL) {
        usb_set_data_payload(usb_device, req, webusb_url_descriptor, sizeof(webusb_url_descriptor));
    } else {
        ret = 0;
    }

    return ret;
}
static void webusb_endpoint_init(struct usb_device_t *usb_device, u32 itf)
{
    const usb_dev usb_id = usb_device2id(usb_device);

    /* usb_g_set_intr_hander(usb_id, WEBUSB_EP_OUT, webusb_epout_isr); */
    usb_g_ep_config(usb_id, WEBUSB_EP_OUT, USB_ENDPOINT_XFER_BULK, 0, webusb_handle->ep_out_dmabuffer, 64);

    usb_g_set_intr_hander(usb_id, WEBUSB_EP_IN | USB_DIR_IN, webusb_epin_isr);
    usb_g_ep_config(usb_id, WEBUSB_EP_IN | USB_DIR_IN, USB_ENDPOINT_XFER_BULK, 1, webusb_handle->ep_in_dmabuffer, 64);

    usb_enable_ep(usb_id, WEBUSB_EP_IN);
}

u32 webusb_tx_data(const usb_dev usb_id, const u8 *buffer, u32 len)
{
    if (webusb_handle == NULL) {
        return -1;
    }
    if (len > MAXP_SIZE_WEBUSBIN) {
        len = MAXP_SIZE_WEBUSBIN;
    }
    return usb_g_intr_write(usb_id, WEBUSB_EP_IN, buffer, len);
}

void webusb_set_rx_hook(void *priv, void (*rx_hook)(void *priv, u8 *buf, u32 len))
{
    webusb_handle->priv_hdl = priv;
    webusb_handle->rx_hook = rx_hook;
}

static void webusb_reset(struct usb_device_t *usb_device, u32 itf)
{
    //cppcheck-suppress unreadVariable
    webusb_endpoint_init(usb_device, itf);
}
static void webusb_epout_isr(struct usb_device_t *usb_device, u32 ep)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u8 buf[64];
    int len = usb_g_bulk_read(usb_id, ep, buf, sizeof(buf), 0);
    if (webusb_handle->rx_hook) {
        webusb_handle->rx_hook(webusb_handle->priv_hdl, buf, len);
    } else {
        log_info("webusb_recv:len = %d\n", len);
    }
}

//发送完成的中断，需要打开ep_config里面的ie，并且注册中断回调函数
static void webusb_epin_isr(struct usb_device_t *usb_device, u32 ep)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u8 buf[64];
    int len = usb_g_bulk_write(usb_id, ep, buf, sizeof(buf));
    log_info("send:len = %d\n", len);
}

//ep0 recv demo
static u8 passwd_verify_ok;
static u32 webusb_passwd_verify(struct usb_device_t *usb_device, struct usb_ctrlrequest *setup)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u32 ret = USB_EP0_STAGE_SETUP;
    u8 read_ep[64];
    u32 len = MIN(setup->wLength, sizeof(read_ep));
    usb_read_ep0(usb_id, read_ep, len);
    ret = USB_EP0_STAGE_SETUP;

    passwd_verify_ok = 0;

    if (memcmp(read_ep, "123", 3) == 0) {
        passwd_verify_ok = 1;
        usb_g_bulk_write(usb_id, WEBUSB_EP_IN, read_ep, sizeof(read_ep));
        printf("passwd verify ok\n");
    }
    setup->wLength -= len;
    if (setup->wLength) {
        return USB_EP0_STAGE_OUT;
    }

    return ret;
}

static u32 webusb_itf_hander(struct usb_device_t *usb_device, struct usb_ctrlrequest *req)
{
    const usb_dev usb_id = usb_device2id(usb_device);
    u32 tx_len;
    u8 *tx_payload = usb_get_setup_buffer(usb_device);
    u32 bRequestType = req->bRequestType & USB_TYPE_MASK;

    switch (bRequestType) {
    case USB_TYPE_STANDARD:
        switch (req->bRequest) {

        case USB_REQ_SET_SECURITY_DATA:
            usb_set_setup_recv(usb_device, webusb_passwd_verify);
            break;

        case USB_REQ_GET_SECURITY_DATA:
            tx_len = req->wLength;
            tx_payload[0] = passwd_verify_ok;
            usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            break;

        case USB_REQ_SET_INTERFACE:
            if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_CONFIGURED) {
                //只有一个interface 没有Alternate
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            }

            break;

        case USB_REQ_GET_INTERFACE:
            if (req->wLength) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_CONFIGURED) {
                tx_len = 1;
                tx_payload[0] = 0x00;
                usb_set_data_payload(usb_device, req, tx_payload, tx_len);
            }

            break;

        case USB_REQ_GET_STATUS:
            if (usb_device->bDeviceStates == USB_DEFAULT) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else if (usb_device->bDeviceStates == USB_ADDRESS) {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            } else {
                usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
            }

            break;

        default:
            usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
        }//bRequest @ USB_TYPE_STANDARD

        break;

    case USB_TYPE_CLASS:
        switch (req->bRequest) {
        default:
            usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
        }//bRequest @ USB_TYPE_CLASS

        break;

    default:
        usb_set_setup_phase(usb_device, USB_EP0_SET_STALL);
    }

    return 0;
}

u32 webusb_desc_config(const usb_dev usb_id, u8 *ptr, u32 *cur_itf_num)
{
    //cppcheck-suppress unreadVariable
    struct usb_device_t *usb_device = usb_id2device(usb_id);
    log_debug("webusb interface num:%d\n", *cur_itf_num);
    memcpy(ptr, sWebUSBDescriptor, sizeof(sWebUSBDescriptor));
    ptr[2] = *cur_itf_num;


    if (usb_set_interface_hander(usb_id, *cur_itf_num, webusb_itf_hander) != *cur_itf_num) {
        ASSERT(0, "webusb set interface_hander fail");
    }

    if (usb_set_reset_hander(usb_id, *cur_itf_num, webusb_reset) != *cur_itf_num) {
        ASSERT(0, "webusb set interface_reset_hander fail");
    }

    webusb_interface_num = *cur_itf_num;
    *cur_itf_num = *cur_itf_num + 1;
    return sizeof(sWebUSBDescriptor);
}


u8 is_webusb_register()
{
    if (webusb_handle) {
        return 1;
    }
    return 0;
}

u32 webusb_register(usb_dev usb_id)
{
    if (webusb_handle) {
        return 0;
    }

    webusb_handle = (struct webusb_handle_t *)zalloc(sizeof(struct webusb_handle_t));
    webusb_handle->ep_in_dmabuffer = usb_alloc_ep_dmabuffer(usb_id, WEBUSB_EP_IN | USB_DIR_IN, 64);
    webusb_handle->ep_out_dmabuffer = usb_alloc_ep_dmabuffer(usb_id, WEBUSB_EP_OUT, 64);
    return 0;
}

void webusb_release(usb_dev usb_id)
{
    if (webusb_handle == NULL) {
        return ;
    }
    usb_free_ep_dmabuffer(usb_id, webusb_handle->ep_in_dmabuffer);
    usb_free_ep_dmabuffer(usb_id, webusb_handle->ep_out_dmabuffer);
    free(webusb_handle);
    webusb_handle = NULL;
}


#endif



