/*
** 包含board的头文件，确定baord里面开关的屏驱宏
*/
#include "app_config.h"

/*
** 驱动代码的宏开关
*/
//<<<[qspi屏 466x466]>>>//
#if TCFG_LCD_QSPI_SD3302_ENABLE


#define LCD_DRIVE_CONFIG                    QSPI_RGB565_SD3302_1T2B

/*
** 包含imd头文件，屏驱相关的变量和结构体都定义在imd.h
*/
#include "asm/imd.h"
#include "asm/imb.h"
#include "includes.h"
#include "ui/ui_api.h"

#define LCD_WIDTH 466
#define LCD_HEIGHT 466

#define SCR_W       454
#define SCR_H       454
#define LCD_W       LCD_WIDTH
#define LCD_H       LCD_HEIGHT
#define SCR_X       (LCD_WIDTH-SCR_W)/2
#define SCR_Y       (LCD_HEIGHT-SCR_H)/2
#define LCD_BLOCK_W LCD_WIDTH
#define LCD_BLOCK_H 16
#define BUF_NUM     2

#define LCD_FORMAT OUTPUT_FORMAT_RGB565

/*
** 初始化代码
*/
static const u8 lcd_qspi_sd3302_cmd_list_poweron[] ALIGNED(4) = {
    _BEGIN_, 0xF0, 0xA5, 0x0F, 0xF0, _END_,
    _BEGIN_, 0xFD, 0x00, _END_,
    _BEGIN_, 0x2A, 0x00, 0x00, 0x01, 0xD1, _END_,
    _BEGIN_, 0x2B, 0x00, 0x00, 0x01, 0xD1, _END_,
    _BEGIN_, 0x35, 0x00, _END_,
    _BEGIN_, 0x3A, 0x55, _END_,
    _BEGIN_, 0x11, _END_,
    _BEGIN_, REGFLAG_DELAY, 50, _END_,
    _BEGIN_, 0xFD, 0xC0, _END_,
    _BEGIN_, 0xD7, 0x16, 0x42, 0x70, 0x44, _END_,
    _BEGIN_, 0xFD, 0x00, _END_,
    _BEGIN_, 0x29, _END_,
    _BEGIN_, 0x51, 0xFF, _END_,

#if 0
    //bist mode
    _BEGIN_, 0xE0, 0x60, _END_,
    _BEGIN_, 0xFD, 0x40, _END_,
    _BEGIN_, 0xF8, 0xF3, _END_,
#endif
};




/*
** lcd背光控制
** 考虑到手表应用lcd背光控制需要更灵活自由，可能需要pwm调光，随时亮灭等
** 因此内部不操作lcd背光，全部由外部自行控制
*/
static int lcd_qspi_sd3302_backlight_ctrl(u8 percent)
{
    if (percent) {

    } else {

    }

    return 0;
}


/*
** lcd电源控制
*/
static int lcd_qspi_sd3302_power_ctrl(u8 onoff)
{
    lcd_en_ctrl(onoff);
    return 0;
}


/* static void delay_2ms(int cnt) */
/* { */
/* if (cnt * 2 > 10) { */
/* os_time_dly(cnt * 2 / 10 + 1); */
/* } else { */
/* mdelay(2 * cnt); */
/* } */
/* } */

/*
** 设置lcd进入睡眠
*/
static void lcd_qspi_sd3302_entersleep(void)
{
    lcd_write_cmd(0x28, NULL, 0);
    lcd_write_cmd(0x10, NULL, 0);
    delay_2ms(120 / 2); // delay 120ms
}



/*
** 设置lcd退出睡眠
*/
static void lcd_qspi_sd3302_exitsleep(void)
{
    lcd_write_cmd(0x11, NULL, 0);
    delay_2ms(5);   // delay 120ms
    lcd_write_cmd(0x29, NULL, 0);
}


struct imd_param lcd_qspi_sd3302_param = {
    .scr_x    = SCR_X,
    .scr_y	  = SCR_Y,
    .scr_w	  = SCR_W,
    .scr_h	  = SCR_H,

    .in_width  = SCR_W,
    .in_height = SCR_H,
    .in_format = LCD_FORMAT,


    .lcd_width  = LCD_W,
    .lcd_height = LCD_H,

    .lcd_type = LCD_TYPE_SPI,

    .buffer_num = BUF_NUM,
    .buffer_size = LCD_BLOCK_W * LCD_BLOCK_H * 2,

    .fps = 30,

    .spi = {
        .spi_mode = SPI_IF_MODE(LCD_DRIVE_CONFIG),
        .pixel_type = PIXEL_TYPE(LCD_DRIVE_CONFIG),
        .out_format = OUT_FORMAT(LCD_DRIVE_CONFIG),
        .port = SPI_PORTA,
        .spi_dat_mode = SPI_MODE_UNIDIR,
        .read_io = IO_PORTA_10,
    },

    .debug_mode_en = false,
    .debug_mode_color = 0xff0000,
};


REGISTER_LCD_DEVICE(sd3302) = {
    .logo = "sd3302",
    .row_addr_align    = 2,
    .column_addr_align = 2,

    .lcd_cmd = (void *) &lcd_qspi_sd3302_cmd_list_poweron,
    .cmd_cnt = sizeof(lcd_qspi_sd3302_cmd_list_poweron) / sizeof(lcd_qspi_sd3302_cmd_list_poweron[0]),
    .param   = (void *) &lcd_qspi_sd3302_param,

    .reset = NULL,  // 没有特殊的复位操作，用内部普通复位函数即可
    .backlight_ctrl = NULL, //lcd_qspi_sd3302_backlight_ctrl,
    .power_ctrl = lcd_qspi_sd3302_power_ctrl,
    .entersleep = lcd_qspi_sd3302_entersleep,
    .exitsleep = lcd_qspi_sd3302_exitsleep,
};


#endif





