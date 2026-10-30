#ifndef  __SD_CARD_H__
#define  __SD_CARD_H__

#include "common.h"


#define SD_DEBUG    1

#if SD_DEBUG
#define sd_puts				puts
#define sd_printf			printf
#else
#define sd_puts(a)
#define sd_printf(...)
#endif


#define s_cmd_r_6byte_busy      1           //48bitack,busy
#define s_cmd_r_17byte_busy     2           //136bitack,busy
#define s_cmd_r_busy            3           //ackbusy
#define s_cmd_r_6byte_nobusy    5           //48bitackbusy
#define s_cmd_r_17byte_nobusy   6           //136bitackbusy
#define s_cmd_nobusy            7           //ackbusy

#define CMD_DUMMY   0
#define CMD0_NR     (((0x40 +  0) << 8) | s_cmd_nobusy          )
#define CMD0_R2     (((0x40 +  0) << 8) | s_cmd_r_17byte_nobusy )
#define CMD1_R1     (((0x40 +  1) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD2_R2     (((0x40 +  2) << 8) | s_cmd_r_17byte_nobusy )
#define CMD3_R6     (((0x40 +  3) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD6_R1     (((0x40 +  6) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD7_R1b    (((0x40 +  7) << 8) | s_cmd_r_6byte_busy    )
#define CMD8_R7     (((0x40 +  8) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD9_R2     (((0x40 +  9) << 8) | s_cmd_r_17byte_nobusy )
#define CMD12_R1b   (((0x40 + 12) << 8) | s_cmd_r_6byte_busy    )
#define CMD13_R1    (((0x40 + 13) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD16_R1    (((0x40 + 16) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD17_R1    (((0x40 + 17) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD18_R1    (((0x40 + 18) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD24_R1    (((0x40 + 24) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD25_R1    (((0x40 + 25) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD41_R3    (((0x40 + 41) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD42_R1    (((0x40 + 42) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD51_R1    (((0x40 + 51) << 8) | s_cmd_r_6byte_nobusy  )
#define CMD55_R1    (((0x40 + 55) << 8) | s_cmd_r_6byte_nobusy  )


typedef enum {
    SD_NULL,                                        //SD
    SDSC_V1,                                        //1.0
    SDSC_V2,                                        //2.0
    SDHC_V2,                                        //2.0
} card_type;

typedef enum {
    SD_V1_0,                                        //1.0 ~ 1.01
    SD_V1_1,                                        //1.1
    SD_V2_0,                                        //2.0
} card_spec;

typedef struct card_info_data {
    card_type type;                                 //
    card_spec spec;                                 //
    u16 rca_id;                                     //ID
    u32 capacity;                                   //
} card_info_type;

struct sd_pdate_type {
    u8 control_type;
    u8 control_io;
    u8 online_check_way;
    u8 max_data_baud;
    u16 wDevTimeOutMax;
    u8 per_online_status;
    u8 hc_mode;
    u8(*io_det_func)(void);
    u8 power;
    u8 control_io_clk;
    u8 control_io_cmd;
    u8 control_io_dat;
};

JL_SD_TypeDef *sd_hw_init(void *sd_pdate);

u8 sd_init_card(void *sd_pdate);

u32 sd_read_card(u8 *buf, u32 sector);

#endif


