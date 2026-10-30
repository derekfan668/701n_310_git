//使用非杰理框架的用户需要重新适配的接口如下：
//
#include "le_smartbox_adv.h"
/* #include "system/fs/fs.h" */
#include "app_config.h"
#include <flashdb.h>


#if (TCFG_PAY_ALIOS_WAY_SEL==TCFG_PAY_ALIOS_WAY_T_HEAD)

#ifndef PARAM_IN
#define PARAM_IN
#endif

#ifndef PARAM_OUT
#define PARAM_OUT
#endif

#ifndef PARAM_INOUT
#define PARAM_INOUT
#endif

#define USE_USER_FS    0 //不是杰理ui框架或者没有文件系统 需要适配
#define USE_USER_TIME  0 //不是使用杰理rtc 的需要额外适配






void upay_recv_data_handle(uint8_t *data, uint16_t len)
{
    extern int upay2ali_ibuf_to_cbuf(u8 * buf, u32 len);
    upay2ali_ibuf_to_cbuf(data, len);
    extern void upay2ali_send_event(void);
    upay2ali_send_event();
}



void alipay_upay_init()
{
    // upay
    upay_ble_regiest_recv_handle(upay_recv_data_handle);
}




#if USE_USER_FS

static struct fdb_kvdb kvdb = { 0 };

static void kvdb_basic_sample(fdb_kvdb_t kvdb)
{
    struct fdb_blob blob;
    int boot_count = 0;

    FDB_INFO("==================== kvdb_basic_sample ====================\n");

    { /* GET the KV value */
        /* get the "boot_count" KV value */
        fdb_kv_get_blob(kvdb, "boot_count", fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));
        /* the blob.saved.len is more than 0 when get the value successful */
        if (blob.saved.len > 0) {
            FDB_INFO("get the 'boot_count' value is %d\n", boot_count);
        } else {
            FDB_INFO("get the 'boot_count' failed\n");
        }
    }

    { /* CHANGE the KV value */
        /* increase the boot count */
        boot_count ++;
        /* change the "boot_count" KV's value */
        fdb_kv_set_blob(kvdb, "boot_count", fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));
        FDB_INFO("set the 'boot_count' value to %d\n", boot_count);
    }

    FDB_INFO("===========================================================\n");
}

#endif




int transit_flashfs_init()
{

#if USE_USER_FS

    fdb_err_t result;
    { /* KVDB Sample */

        /* set the lock and unlock function if you want */
        /* fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_LOCK, lock); */
        /* fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_UNLOCK, unlock); */

        int sec_size = 4096 * 2;

        fdb_kvdb_control(&kvdb, FDB_KVDB_CTRL_SET_SEC_SIZE, &sec_size);

        /* Key-Value database initialization
         *
         *       &kvdb: database object
         *       "env": database name
         * "fdb_kvdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         * &default_kv: The default KV nodes. It will auto add to KVDB when first initialize successfully.
         *        NULL: The user data if you need, now is empty.
         */

        result = fdb_kvdb_init(&kvdb, "env", "transit_fal", NULL, NULL);

        if (result != FDB_NO_ERR) {
            return -1;
        }
        /* run basic KV samples */
        kvdb_basic_sample(&kvdb);
    }

#endif
    return 0;
}



#if TCFG_PAY_TRANSITCODE_ENABLE

//使用了文件系统方案的更新以下路径即可，不需要重新适配文件系统接口
char *alipay_get_rsvd_root()
{
    return "storage/virfat_flash/C/";
}



#if USE_USER_FS

void *alipay_open_rsvd_part(PARAM_IN char filename[36])
{
    printf("alipay_open %s\n", filename);
    u8 *name = malloc(strlen(filename) + 1);
    sprintf(name, "%s", filename);
    return name;

}

/*
 * 写文件
 *
 * parametr: in: fd: 文件描述符
 *               offset: 地址偏移
 *               data: 要写的数据
 *               data_len: 要写的数据长度
 *
 * return: 0: 表示成功
 *         -1: 表示失败
*/

int alipay_write_rsvd_part(PARAM_IN void *fd, PARAM_IN void *data, PARAM_IN uint32_t data_len)
{
    int ret;
    ASSERT(fd);
    int offset = 0;
    /* printf(" %s %s %x %d %d \n", __FUNCTION__, fd, data, offset, data_len); */
    ASSERT(data_len < 1024 * 7);
    /* put_buf(data, data_len); */
    struct fdb_blob blob;
    int data_read_len;
    struct fdb_kv kv;
    u8 *temp_data;//
    if (fdb_kv_get_obj(&kvdb, fd, &kv)) {
        if (kv.value_len > offset + data_len) {
            data_read_len = kv.value_len;
        } else {
            data_read_len = offset + data_len;
        }
        temp_data = zalloc(data_read_len);
        fdb_kv_get_blob(&kvdb, fd, fdb_blob_make(&blob, temp_data, kv.value_len));
    } else {
        data_read_len = offset + data_len;
        temp_data = zalloc(data_read_len);
    }
    memcpy(&temp_data[offset], data, data_len);
    fdb_kv_set_blob(&kvdb, fd, fdb_blob_make(&blob, temp_data, data_read_len));
    free(temp_data);
    return 0;
}


/*
 * 读文件
 *
 * parametr: in:  fd: 文件描述符
 *                read_len: 要读取的长度
 *           out: buffer: 读取的数据所存放的区域

 * return: 0: 表示成功
           -1: 表示失败
 */

int alipay_read_rsvd_part(PARAM_IN void *fd, PARAM_OUT void *buffer, PARAM_INOUT uint32_t *read_len)
{
    int offset = 0;
    ASSERT(fd);
    /* printf("%s ,%s,%x %d %d \n", __FUNCTION__, fd, buffer, offset, *read_len); */
    struct fdb_blob blob;

    u8 *temp_data = zalloc(offset + *read_len);
    fdb_kv_get_blob(&kvdb, fd, fdb_blob_make(&blob, temp_data, offset + *read_len));
    if (blob.saved.len > offset) {
        if (blob.saved.len < offset + *read_len) {
            *read_len = blob.saved.len;
        }
        memcpy(buffer, &temp_data[offset], *read_len);
    } else {
        free(temp_data);
        *read_len = 0;
        return -1;
    }
    free(temp_data);
    /* put_buf(buffer, *read_len); */

    return 0;
}
/*
 * 关闭文件
 *
 * parametr: in: fd: 文件描述符

 * return: 0: 表示成功
           -1: 表示失败
 */

int alipay_close_rsvd_part(PARAM_IN void *fd)
{
    /* printf("%s \n", __FUNCTION__); */
    free(fd);
    return 0;
}

/*
 * 查看文件是否存在
 *
 * parametr: in: filename: 文件名(字符串)

 * return: 0: 文件存在
           -1: 文件不存在
 */

int alipay_access_rsvd_part(PARAM_IN char filename[36])
{
    /* printf("check_open %s\n", filename); */
    struct fdb_blob blob;
    u8 temp_data;
    fdb_kv_get_blob(&kvdb, filename, fdb_blob_make(&blob, &temp_data, 1));
    if (blob.saved.len > 0) {
        return 0;
    }
    return -1;

}

/*
 * 删除文件
 *
 * parametr: in: filename: 文件名(字符串)

 * return: 0: 删除成功
           -1: 删除失败
 */

int alipay_remove_rsvd_part(PARAM_IN char filename[36])
{
    /* printf("%s\n", __FUNCTION__); */
    fdb_kv_del(&kvdb, filename);
    return 0;
}

/*
 * 清除所有通过alipay_write_rsvd_part写的alipay文件
 *
 * parametr:

 * return: 0: 清除成功
           -1: 清除失败
           */

int alipay_clear_rsvd_part(void)
{
    /* printf("%s\n", __FUNCTION__); */
    struct fdb_kv_iterator iterator;
    fdb_kv_t cur_kv;
    /* struct fdb_blob blob; */
    /* size_t data_size; */
    /* uint8_t *data_buf; */

    fdb_kv_iterator_init(&iterator);
    while (fdb_kv_iterate(&kvdb, &iterator)) {
        cur_kv = &(iterator.curr_kv);
        /* data_size = (size_t) cur_kv->value_len; */
        /* data_buf = (uint8_t *) malloc(data_size); */
        /* if (data_buf == NULL) { */
        /*     FDB_INFO("Error: malloc failed.\n"); */
        /*     break; */
        /* } */
        /* fdb_blob_read((fdb_db_t) &kvdb, fdb_kv_to_blob(cur_kv, fdb_blob_make(&blob, data_buf, data_size))); */
        printf("name %s \n", cur_kv->name);
        printf("len  %x \n", cur_kv->value_len);
        fdb_kv_del(&kvdb, cur_kv->name);
        /*
         * balabala do what ever you like with blob...
         */
        /* free(data_buf); */
    }
    return 0;
}



#endif



#if USE_USER_TIME
int transit_time_get_support(struct sys_time *time)
{
    return true;
}
#endif


#endif
#endif


