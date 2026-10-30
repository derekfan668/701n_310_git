#include "usb/scsi.h"
#include "asm/uart.h"
#include "asm/usb.h"
#include "usb/usb_host.h"
#include "asm/irq.h"
#include "asm/gpio.h"

#if 1//TCFG_UDISK_ENABLE || TCFG_ADB_ENABLE ||TCFG_AOA_ENABLE || TCFG_HID_HOST_ENABLE

#if TCFG_HID_HOST_ENABLE
#define     MAX_HOST_EP_RX  4
#define     MAX_HOST_EP_TX  4
#else
#define     MAX_HOST_EP_RX  2
#define     MAX_HOST_EP_TX  2   //ep0 & ep1(msd)
#endif

struct host_var_t {
    struct usb_ep_addr_t host_ep_addr ;
    usb_h_interrupt usb_h_interrupt_rx[MAX_HOST_EP_RX] ;
    usb_h_interrupt usb_h_interrupt_tx[MAX_HOST_EP_TX] ;
    struct usb_host_device *dev_at_ep[MAX_HOST_EP_RX];
};
static struct host_var_t *host_var[USB_MAX_HW_NUM];// SEC(.usb_h_bss);
static struct host_var_t __host_var[USB_MAX_HW_NUM];
static u8 msd_h_dma_buffer[1][64 + 4]  __attribute__((aligned(4)));// SEC(.usb_h_dma);

void usb_h_isr(const usb_dev usb_id)
{
    u32 intr_usb, intr_usbe;
    u32 intr_tx, intr_txe;
    u32 intr_rx, intr_rxe;

    __asm__ volatile("ssync");

    usb_read_intr(usb_id, &intr_usb, &intr_tx, &intr_rx);
    usb_read_intre(usb_id, &intr_usbe, &intr_txe, &intr_rxe);
    struct usb_host_device *host_dev = NULL;

    /* r_usb_printf("usb_h_isr %x %x %x %x",host_dev,intr_usb,intr_tx,intr_rx); */
    intr_usb &= intr_usbe;
    intr_tx &= intr_txe;
    intr_rx &= intr_rxe;

    if (intr_usb & INTRUSB_SUSPEND) {
        usb_printf("usb suspend");
    }
    if (intr_usb & INTRUSB_RESET_BABBLE) {
        usb_printf("usb reset");
    }
    if (intr_usb & INTRUSB_RESUME) {
        usb_printf("usb resume");
    }

#if 0
    if (intr_tx & BIT(0)) {
        if (host_var[usb_id]->usb_h_interrupt_tx[0]) {
            host_dev = host_var[usb_id]->dev_at_ep[0];
            host_var[usb_id]->usb_h_interrupt_tx[0](host_dev, 0);
        }
    }
#endif

    for (int i = 1; i < MAX_HOST_EP_TX; i++) {
        if (intr_tx & BIT(i)) {
            if (host_var[usb_id]->usb_h_interrupt_tx[i]) {
                host_dev = host_var[usb_id]->dev_at_ep[i];
                host_var[usb_id]->usb_h_interrupt_tx[i](host_dev, i);
            }
        }
    }

    for (int i = 1; i < MAX_HOST_EP_RX; i++) {
        if (intr_rx & BIT(i)) {
            if (host_var[usb_id]->usb_h_interrupt_rx[i]) {
                host_dev = host_var[usb_id]->dev_at_ep[i];
                host_var[usb_id]->usb_h_interrupt_rx[i](host_dev, i);
            }
        }
    }
    __asm__ volatile("csync");
}
SET_INTERRUPT
void usb0_h_isr()
{
    usb_h_isr(0);
}
__attribute__((always_inline_when_const_args))
u32 usb_h_set_intr_hander(const usb_dev usb_id, u32 ep, usb_h_interrupt hander)
{
    if (ep & USB_DIR_IN) {
        host_var[usb_id]->usb_h_interrupt_rx[ep & 0xf] = hander;
    } else {
        host_var[usb_id]->usb_h_interrupt_tx[ep] = hander;
    }
    return 0;
}
void usb_h_isr_reg(const usb_dev usb_id, u8 priority, u8 cpu_id)
{
    if (usb_id == 0) {
        request_irq(22, priority, usb0_h_isr, cpu_id);
    }
}

__attribute__((always_inline_when_const_args))
void *usb_h_get_ep_buffer(const usb_dev usb_id, u32 ep)
{
    return msd_h_dma_buffer;
}
void usb_h_set_ep_isr(struct usb_host_device *host_dev, u32 ep, usb_h_interrupt hander, void *p)
{
    if (host_dev) {
        usb_dev usb_id = host_device2id(host_dev);
        host_var[usb_id]->dev_at_ep[ep & 0xf] = p;
        usb_h_set_intr_hander(usb_id, ep, hander);
    }
}
void usb_host_config(usb_dev usb_id)
{
    /* host_var[usb_id] = zalloc(sizeof(struct host_var_t)); */
    host_var[usb_id] = &__host_var[usb_id];

    /* ASSERT(host_var[usb_id], "host_var_t"); */

    usb_var_init(usb_id, &(host_var[usb_id]->host_ep_addr));
}
#endif
