#include "typedef.h"
#include "device/device.h"
#include "fal.h"




const struct fal_partition *fal_vm_partition_find(char *name)
{
    static void *fd;
    struct fal_partition *hd;
    const struct flashdb_vm_info *p;
    int offset = 0;

    //這裏是找客户的
    if (!strcmp(name, "transit_fal")) {
        hd = (struct fal_partition *)zalloc(sizeof(struct fal_partition));
        hd->fd = dev_open("transit_fal", NULL);
        fd = hd->fd;
        ASSERT(hd);
        hd->offset = 0;//
        hd->len = 1024 * 1024; //
        return hd;
    }
    ASSERT(0, "Please make sure %s it's in FAL partition table\n", name);
    return NULL;
}



/**
 *
 * read data from partition
 *
 * @param part partition
 * @param addr relative address for partition
 * @param buf read buffer
 * @param size read size
 *
 * @return >= 0: successful read data size
 *           -1: error
 */
int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    /* printf("%s %x %x %x\n",__FUNCTION__,rets,addr,size); */
    if (!part || !part->fd) {
        return -1;
    }
    return dev_bulk_read(part->fd, buf, addr + part->offset, size);
}

/**
 * write data to partition
 *
 * @param part partition
 * @param addr relative address for partition
 * @param buf write buffer
 * @param size write size
 *
 * @return >= 0: successful write data size
 *           -1: error
 */
int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size)
{
    u32 rets;//, reti;
    __asm__ volatile("%0 = rets":"=r"(rets));
    /* printf("%s %x %x %x\n",__FUNCTION__,rets,addr,size); */
    if (!part || !part->fd) {
        return -1;
    }
    return dev_bulk_write(part->fd, buf, addr + part->offset, size);
}


/**
 * erase partition data
 *
 * @param part partition
 * @param addr relative address for partition
 * @param size erase size
 *
 * @return >= 0: successful erased data size
 *           -1: error
 */
int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size)
{
    if (!part || !part->fd) {
        return -1;
    }
    do {
        dev_ioctl(part->fd, IOCTL_ERASE_SECTOR, addr + part->offset);
        addr += 4096;
        size -= 4096;
    } while (size > 0);
    return 0;
}

/**
 * erase partition all data
 *
 * @param part partition
 *
 * @return >= 0: successful erased data size
 *           -1: error
 */



#include <flashdb.h>



#if 1
void kvdb_basic_sample(fdb_kvdb_t kvdb)
{
    struct fdb_blob blob;
    int boot_count = 0;

    FDB_INFO("==================== kvdb_basic_sample ====================\n");
#if 1

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
#endif
}

#endif

#if 1
void kvdb_type_string_sample(fdb_kvdb_t kvdb)
{
    FDB_INFO("==================== kvdb_type_string_sample ====================\n");
    { /* CREATE new Key-Value */
        char temp_data[10] = "36C";

        /* It will create new KV node when "temp" KV not in database. */
        fdb_kv_set(kvdb, "tem", temp_data);
        FDB_INFO("create the 'temp' string KV, value is: %s\n", temp_data);
    }

    { /* GET the KV value */
        char *return_value, temp_data[10] = { 0 };

        /* Get the "temp" KV value.
         * NOTE: The return value saved in fdb_kv_get's buffer. Please copy away as soon as possible.
         */
        return_value = fdb_kv_get(kvdb, "tem");
        /* the return value is NULL when get the value failed */
        if (return_value != NULL) {
            strncpy(temp_data, return_value, sizeof(temp_data));
            FDB_INFO("get the 'temp' value is: %s\n", temp_data);
        }
    }

    { /* CHANGE the KV value */
        char temp_data[10] = "38C";

        /* change the "temp" KV's value to "38C" */
        fdb_kv_set(kvdb, "tem", temp_data);
        FDB_INFO("set 'temp' value to %s\n", temp_data);
    }

    { /* DELETE the KV by name */
        fdb_kv_del(kvdb, "tem");
        FDB_INFO("delete the 'temp' finish\n");
    }

    FDB_INFO("===========================================================\n");
}
#endif


#if 1

void kvdb_type_blob_sample(fdb_kvdb_t kvdb)
{
    struct fdb_blob blob;

    FDB_INFO("==================== kvdb_type_blob_sample ====================\n");

    { /* CREATE new Key-Value */
        int temp_data = 36;

        /* It will create new KV node when "temp" KV not in database.
         * fdb_blob_make: It's a blob make function, and it will return the blob when make finish.
         */
        fdb_kv_set_blob(kvdb, "temp", fdb_blob_make(&blob, &temp_data, sizeof(temp_data)));
        FDB_INFO("create the 'temp' blob KV, value is: %d\n", temp_data);
    }

    { /* GET the KV value */
        int temp_data = 0;

        /* get the "temp" KV value */
        fdb_kv_get_blob(kvdb, "temp", fdb_blob_make(&blob, &temp_data, sizeof(temp_data)));
        /* the blob.saved.len is more than 0 when get the value successful */
        if (blob.saved.len > 0) {
            FDB_INFO("get the 'temp' value is: %d\n", temp_data);
        }
    }

    { /* CHANGE the KV value */
        int temp_data = 38;

        /* change the "temp" KV's value to 38 */
        fdb_kv_set_blob(kvdb, "temp", fdb_blob_make(&blob, &temp_data, sizeof(temp_data)));
        FDB_INFO("set 'temp' value to %d\n", temp_data);
    }

    { /* DELETE the KV by name */
        fdb_kv_del(kvdb, "temp");
        FDB_INFO("delete the 'temp' finish\n");
    }

    FDB_INFO("===========================================================\n");
}

#endif






#if 0

void kvdb_tarversal_sample(fdb_kvdb_t kvdb)
{
    struct fdb_kv_iterator iterator;
    fdb_kv_t cur_kv;
    struct fdb_blob blob;
    size_t data_size;
    uint8_t *data_buf;

    fdb_kv_iterator_init(kvdb, &iterator);

    while (fdb_kv_iterate(kvdb, &iterator)) {
        cur_kv = &(iterator.curr_kv);
        data_size = (size_t) cur_kv->value_len;
        data_buf = (uint8_t *) malloc(data_size);
        if (data_buf == NULL) {
            FDB_INFO("Error: malloc failed.\n");
            break;
        }
        fdb_blob_read((fdb_db_t) kvdb, fdb_kv_to_blob(cur_kv, fdb_blob_make(&blob, data_buf, data_size)));
        put_buf(data_buf, data_size);
        /*
         *
         * balabala do what ever you like with blob...
         */
        free(data_buf);
    }
}
#endif


struct env_status {
    int temp;
    int humi;
};

static bool query_cb(fdb_tsl_t tsl, void *arg);
static bool query_by_time_cb(fdb_tsl_t tsl, void *arg);
static bool set_status_cb(fdb_tsl_t tsl, void *arg);


static bool query_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    struct env_status status;
    fdb_tsdb_t db = arg;

    fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &status, sizeof(status))));
    FDB_INFO("[query_cb] queried a TSL: time: %ld, temp: %d, humi: %d\n", tsl->time, status.temp, status.humi);
    printf("!!!!!!! %x %x %x\n", blob.saved.meta_addr, blob.saved.addr, blob.saved.len);


    return false;
}

static bool query_by_time_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    struct env_status status;
    fdb_tsdb_t db = arg;

    fdb_blob_read((fdb_db_t) db, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &status, sizeof(status))));
    FDB_INFO("[query_by_time_cb] queried a TSL: time: %ld, temp: %d, humi: %d\n", tsl->time, status.temp, status.humi);

    return false;
}

static bool set_status_cb(fdb_tsl_t tsl, void *arg)
{
    fdb_tsdb_t db = arg;

    FDB_INFO("set the TSL (time %ld) status from %d to %d\n", tsl->time, tsl->status, FDB_TSL_USER_STATUS1);
    fdb_tsl_set_status(db, tsl, FDB_TSL_USER_STATUS1);

    return false;
}




#include "system/sys_time.h"

extern u32 timestamp_mytime_2_utc_sec(struct sys_time *mytime);

time_t flashdb_mktime(struct tm *__tp)
{
    struct sys_time time;
    time.year = __tp->tm_year;
    time.month = __tp->tm_mon;
    time.day = __tp->tm_mday;
    time.hour = __tp->tm_hour;
    time.min = __tp->tm_min;
    time.sec = __tp->tm_sec;
    uint32_t utime = timestamp_mytime_2_utc_sec(&time);
    return utime;
}




void tsdb_sample(fdb_tsdb_t tsdb, int test)
{
    struct fdb_blob blob;

    FDB_INFO("==================== tsdb_sample ====================\n");

    { /* APPEND new TSL (time series log) */
        struct env_status status;

        if (test) {
            /* append new log to TSDB */
            status.temp = 36;
            status.humi = 85;
            fdb_tsl_append(tsdb, fdb_blob_make(&blob, &status, sizeof(status)));
            FDB_INFO("append the new status.temp (%d) and status.humi (%d)\n", status.temp, status.humi);

            status.temp = 38;
            status.humi = 90;
            fdb_tsl_append(tsdb, fdb_blob_make(&blob, &status, sizeof(status)));
            FDB_INFO("append the new status.temp (%d) and status.humi (%d)\n", status.temp, status.humi);
        } else {

            status.temp = 12;
            status.humi = 34;
            fdb_tsl_append(tsdb, fdb_blob_make(&blob, &status, sizeof(status)));
            FDB_INFO("append the new status.temp (%d) and status.humi (%d)\n", status.temp, status.humi);

            status.temp = 56;
            status.humi = 78;
            fdb_tsl_append(tsdb, fdb_blob_make(&blob, &status, sizeof(status)));
            FDB_INFO("append the new status.temp (%d) and status.humi (%d)\n", status.temp, status.humi);

        }
    }

    { /* QUERY the TSDB */
        /* query all TSL in TSDB by iterator */
        fdb_tsl_iter(tsdb, query_cb, tsdb);
    }

    { /* QUERY the TSDB by time */
        /* prepare query time (from 1970-01-01 00:00:00 to 2020-05-05 00:00:00) */
        struct tm tm_from = { .tm_year = 1970 - 1900, .tm_mon = 0, .tm_mday = 1, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
        struct tm tm_to = { .tm_year = 2020 - 1900, .tm_mon = 4, .tm_mday = 5, .tm_hour = 0, .tm_min = 0, .tm_sec = 0 };
        time_t from_time = flashdb_mktime(&tm_from), to_time = flashdb_mktime(&tm_to);
        size_t count;
        /* query all TSL in TSDB by time */
        fdb_tsl_iter_by_time(tsdb, from_time, to_time, query_by_time_cb, tsdb);
        /* query all FDB_TSL_WRITE status TSL's count in TSDB by time */
        count = fdb_tsl_query_count(tsdb, from_time, to_time, FDB_TSL_WRITE);
        FDB_INFO("query count is: %u\n", count);
    }

    { /* SET the TSL status */
        /* Change the TSL status by iterator or time iterator
         * set_status_cb: the change operation will in this callback
         *
         * NOTE: The actions to modify the state must be in orderC.
         *       like: FDB_TSL_WRITE -> FDB_TSL_USER_STATUS1 -> FDB_TSL_DELETED -> FDB_TSL_USER_STATUS2
         *       The intermediate states can also be ignored.
         *       such as: FDB_TSL_WRITE -> FDB_TSL_DELETED
         */
        fdb_tsl_iter(tsdb, set_status_cb, tsdb);
    }

    FDB_INFO("===========================================================\n");
}


static uint32_t boot_count = 0;
static time_t boot_time[10] = {0, 1, 2, 3};


static int counts = 0;

static fdb_time_t get_time(void)
{
    /* Using the counts instead of timestamp.
     * Please change this function to return RTC time.
     */
    return ++counts;
}




static struct fdb_default_kv_node default_kv_table[] = {
    {"username", "armink", 0}, /* string KV */
    {"password", "123456", 0}, /* string KV */
    {"boot_count", &boot_count, sizeof(boot_count)}, /* int type KV */
    {"boot_time", &boot_time, sizeof(boot_time)},    /* int array type KV */
    {"12345678", "112233445556", 7},    /* int array type KV */
};

/* KVDB object */
static struct fdb_kvdb kvdb = { 0 };

/* TSDB object */
struct fdb_tsdb tsdb = { 0 };

/* TSDB object */
struct fdb_tsdb tsdb_call = { 0 };


int test_flashdb()
{
#ifdef FDB_USING_KVDB
    fdb_err_t result;
    { /* KVDB Sample */
        struct fdb_default_kv default_kv;

        default_kv.kvs = default_kv_table;
        default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

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

        result = fdb_kvdb_init(&kvdb, "env", "fdb_kvdb1", &default_kv, NULL);

        if (result != FDB_NO_ERR) {
            return -1;
        }

        /* run basic KV samples */
        kvdb_basic_sample(&kvdb);
        /* run string KV samples */
        kvdb_type_string_sample(&kvdb);
        /* run blob KV samples */
        kvdb_type_blob_sample(&kvdb);

        /* kvdb_tarversal_sample(&kvdb); */
    }
#endif /* FDB_USING_KVDB */


#ifdef FDB_USING_TSDB
    { /* TSDB Sample */
        /* set the lock and unlock function if you want */
        /* fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_LOCK, lock); */
        /* fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_SET_UNLOCK, unlock); */
        /* Time series database initialization
         *
         *       &tsdb: database object
         *       "log": database name
         * "fdb_tsdb1": The flash partition name base on FAL. Please make sure it's in FAL partition table.
         *              Please change to YOUR partition name.
         *    get_time: The get current timestamp function.
         *         128: maximum length of each log
         *        NULL: The user data if you need, now is empty.
         */
        result = fdb_tsdb_init(&tsdb, "log", "fdb_tsdb1", get_time, 128, NULL);
        /* read last saved time for simulated timestamp */
        fdb_tsdb_control(&tsdb, FDB_TSDB_CTRL_GET_LAST_TIME, &counts);

        if (result != FDB_NO_ERR) {
            return -1;
        }

        /* run TSDB sample */
        tsdb_sample(&tsdb, 1);


        result = fdb_tsdb_init(&tsdb_call, "log_b", "fdb_tsdb2", get_time, 128, NULL);
        /* read last saved time for simulated timestamp */
        fdb_tsdb_control(&tsdb_call, FDB_TSDB_CTRL_GET_LAST_TIME, &counts);

        if (result != FDB_NO_ERR) {
            ASSERT(0);
            return -1;
        }

        /* run TSDB sample */
        tsdb_sample(&tsdb_call, 0);

    }
#endif /* FDB_USING_TSDB */


    return 0;

    /* extern void test_file(); */
    /* test_file(); */

    return 0;
}

/* #include "file_demo.c"  */

