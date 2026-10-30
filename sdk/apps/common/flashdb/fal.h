#ifndef __FAL_V_H__
#define __FAL_V_H__

struct fal_partition {
    void *fd;
    /* uint32_t magic_word; */

    /* partition name */
    /* char name[FAL_DEV_NAME_MAX]; */
    /* flash device name for partition */
    /* char flash_name[FAL_DEV_NAME_MAX]; */

    /* partition offset address on flash device */
    long offset;
    size_t len;

    /* uint32_t reserved; */
};




const struct fal_partition *fal_vm_partition_find(char *name);


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
int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size);


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
int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size);

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
int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size);
/**
 * erase partition all data
 *
 * @param part partition
 *
 * @return >= 0: successful erased data size
 *           -1: error
 */


#endif

