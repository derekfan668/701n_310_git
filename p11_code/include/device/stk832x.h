#pragma once
#ifdef TCFG_STK832X_EN && TCFG_STK832X_EN
#define STK832x_REG_RANGESEL    0x0F
#define STK832x_RANGE_2G        0x03
#define STK832x_RANGE_4G        0x05
#define STK832x_RANGE_8G        0x08

#define STK_REG_POWMODE         0x11
#define STK_SUSPEND_MODE        0x80

#define STK832x_SET_ODR_25HZ
#define STK832x_SET_ODR_50HZ
#define STK832x_SET_ODR_100HZ

#define STK_REG_CHIPID          0x00
#define STK8327_CHIPID_VAL      0x26	/* STK8327 CHIP ID = 0x26 */
#define STK8329_CHIPID_VAL      0x25	/* STK8329&STK8325 CHIP ID = 0x25 */
#define STK832x_CHIPID_VAL      0x23    /* STK8321 or STK8323 CHIP ID = 0x23 */
#define FIFO_FRAME_MAX_CNT      32

#endif
