#include "delay.h"

void rc_udelay(u32 usec)
{
    JL_TIMER0->CON = BIT(14);
    JL_TIMER0->CNT = 0;
    JL_TIMER0->PRD = 16 / 4 * usec; //1us
    JL_TIMER0->CON = BIT(0) | (0b0001 << 4) | (4 << 10);//rc16
    while ((JL_TIMER0->CON & BIT(15)) == 0);
    JL_TIMER0->CON = BIT(14);
}

void udelay(u32 usec)
{
    JL_TIMER0->CON = BIT(14);
    JL_TIMER0->CNT = 0;
    JL_TIMER0->PRD = 24 / 4 * usec; //1us
    JL_TIMER0->CON = BIT(0) | (0b0001 << 4) | (6 << 10);//std24
    while ((JL_TIMER0->CON & BIT(15)) == 0);
    JL_TIMER0->CON = BIT(14);
}

void mdelay(u32 msec)
{
    for (int i = 0; i < msec; i++) {
        udelay(1000);
    }
}

void delay(volatile u32 t)
{
    while (t--) {
        asm("nop");
    }
}


//volatile unsigned long jiffies = 10;

