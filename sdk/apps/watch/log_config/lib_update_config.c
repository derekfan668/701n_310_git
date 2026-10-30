
#include "app_config.h"
#include "system/includes.h"
#include "update_loader_download.h"

#ifdef CONFIG_256K_FLASH
const int config_update_mode = UPDATE_UART_EN;
#else
const int config_update_mode = UPDATE_BT_LMP_EN | UPDATE_STORAGE_DEV_EN | UPDATE_APP_EN | UPDATE_UART_EN;
#endif

//是否采用双备份升级方案:0-单备份;1-双备份
#if CONFIG_DOUBLE_BANK_ENABLE
const int support_dual_bank_update_en = 1;
#else
const int support_dual_bank_update_en = 0;
#endif

//双备份模式下是否采用使用ota包小包方式，适用于双备份差分升级,手机OTA等
#if CONFIG_UPDATE_USE_SMALL_PACKAGE
const int support_db_update_small_package = 1;
#else
const int support_db_update_small_package = 0;
#endif

//是否支持外挂flash升级,需要打开Board.h中的TCFG_NOR_FS_ENABLE
const int support_norflash_update_en  = 0;

//支持从外挂flash读取ufw文件升级使能
const int support_norflash_ufw_update_en = 0;

//支持从内置flash读取ufw文件升级使能
const int support_lc_flash_ufw_update_en = 0;

//支持从nand_flash读取ufw文件升级使能
const int support_nandflash_ufw_update_en = 0;

//支持计算推loader数据时的进度
const int support_update_loader_precent = 0;

//支持对btif区域中的蓝牙地址升级前备份，升级后还原的操作
const int support_update_backup_btif_addr = 1;

//支持根据外挂flash完整的ota.bin文件，直接走推送loader的流程
const int support_norflash_update_loader_only = 0;

// 是否支持双备份断点续传升级
const int support_dual_bank_update_breakpoint = 0;

//支持外挂flash和预留区域一起升级(如果只升级外挂flash不升级预留区，只需要打包升级文件时不打包预留区文件即可)
const int support_user_file_update_v2_en = 1;

//是否双备份升级模式下不升级预留区域，1为不升级
const int support_dual_bank_not_update_reserve = 0;

//支持双备份升级模式但app1区域比app0小，适用于压缩升级功能
const int support_dual_bank_less_en = 0;

#if OTA_TWS_SAME_TIME_NEW       //使用新的同步升级流程
const int support_ota_tws_same_time_new =  1;
#else
const int support_ota_tws_same_time_new =  0;
#endif

//是否支持升级之后保留vm数据
#if (((defined CONFIG_CPU_BR25) || defined(CONFIG_CPU_BR28)) && SMART_BOX_EN)
const int support_vm_data_keep = 1;
#else
const int support_vm_data_keep = 0;
#endif

const char log_tag_const_v_UPDATE AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_i_UPDATE AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;
const char log_tag_const_d_UPDATE AT(.LOG_TAG_CONST) = LIB_DEBUG &  FALSE;
const char log_tag_const_w_UPDATE AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;
const char log_tag_const_e_UPDATE AT(.LOG_TAG_CONST) = LIB_DEBUG &  TRUE;
