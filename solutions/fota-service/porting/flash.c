/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <linux/fs.h>
#include <fcntl.h>
#include <errno.h>
#include <aos/kernel.h>
#include <aos/kv.h>
#include <yoc/netio.h>
#include <yoc/fota.h>
#include <ulog/ulog.h>
#include <mtd/ubi-user.h>
#include <mtd/mtd-user.h>
#include "imagef.h"

#define TAG "fota"

struct partition_info_t {
    char img_name[IMG_NAME_MAX_LEN];
    char char_name[DEV_NAME_MAX_LEN + 4]; // for uboot name(/dev/mmcblk0boot0) length + 4
    char dev_name[DEV_NAME_MAX_LEN + 4];
    size_t size;
    int ab;
};

static struct partition_info_t g_partition_info_ubi[] = {
    {IMG_NAME_UBOOT,  "/dev/mtd1", "/dev/mtdblock1", ( 4 * 1024 * 1024), 1},
    {IMG_NAME_KERNEL, "", "/dev/ubi0_5",    (10 * 1024 * 1024), 1},
    {IMG_NAME_KERNEL, "", "/dev/ubi0_6",    (10 * 1024 * 1024), 2},
    {IMG_NAME_ROOTFS, "", "/dev/ubi0_7",    (60 * 1024 * 1024), 1},
    {IMG_NAME_ROOTFS, "", "/dev/ubi0_8",    (60 * 1024 * 1024), 2},
    {"", "", "", 0, 0},
};

static struct partition_info_t g_partition_info_ext4_uboot[] = {
    {IMG_NAME_UBOOT,  "/dev/mmcblk0boot0", "/dev/mmcblk0boot0", ( 4 * 1024 * 1024), 1},
};

static struct partition_info_t *g_partition_info;

// data: if return 0, need free *data
static int get_emmc_valid_partition_info(struct partition_info_t **data)
{
    FILE *fp;
    char buf[60];
    char part_name[10];
    char *p;
    struct partition_info_t *out_data;
    int all;
    int nums;
    int i;
    int len;
    int cnt;
    int tmp;
    int tmp2;

    fp = popen("cd /sys/block/mmcblk0/ && ls mmcblk0p*/uevent", "r");
    if (fp == NULL) {
        LOGE(TAG, "popen error.");
        return -1;
    }
    all = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        all++;
    }
    pclose(fp);
    for (i = 0, nums = 0; i < all; i++) {
        snprintf(buf, sizeof(buf), "/sys/block/mmcblk0/mmcblk0p%d/uevent", i + 1);
        fp = fopen(buf, "r");
        if (fp == NULL) {
            LOGE(TAG, "popen %s error.", buf);
            return -1;
        }
        while (fgets(buf, sizeof(buf), fp)) {
            p = strstr(buf, "PARTNAME=");
            if (p) {
                if (sscanf(p, "PARTNAME=%[^\n]", part_name) == 1) {
                    len = strlen(part_name);
                    if (len <= 0) {
                        LOGE(TAG, "part_name analysis error:%s", p);
                        pclose(fp);
                        return -1;
                    }
                    if (part_name[len - 1] == 'B') {
                        part_name[len - 1] = 0;
                    }
                    if (check_partition_exists(part_name)) {
                        nums++;
                    }
                    break;
                }
            }
        }
        pclose(fp);
    }
    if (nums == 0) {
        LOGE(TAG, "get part error.");
        return -1;
    }
    tmp2 = sizeof(g_partition_info_ext4_uboot) / sizeof(g_partition_info_ext4_uboot[0]);
    out_data = aos_malloc(sizeof(struct partition_info_t) * (nums + tmp2 + 1));
    if (out_data == NULL) {
        LOGE(TAG, "struct partition_info_t aos_malloc error.");
        return -1;
    }
    memcpy(out_data, g_partition_info_ext4_uboot, sizeof(*out_data) * tmp2);
    for (i = 0, cnt = 0; i < all; i++) {
        snprintf(buf, sizeof(buf), "/sys/block/mmcblk0/mmcblk0p%d/uevent", i + 1);
        fp = fopen(buf, "r");
        if (fp == NULL) {
            LOGE(TAG, "popen %s error.", buf);
            goto err;
        }
        while (fgets(buf, sizeof(buf), fp)) {
            p = strstr(buf, "PARTNAME=");
            if (p) {
                if (sscanf(p, "PARTNAME=%[^\n]", part_name) == 1) {
                    len = strlen(part_name);
                    if (len <= 0) {
                        LOGE(TAG, "part_name analysis error:%s", p);
                        pclose(fp);
                        goto err;
                    }
                    if (part_name[len - 1] == 'B') {
                        part_name[len - 1] = 0;
                        tmp = 2;
                    } else {
                        tmp = 1; 
                    }
                    if (check_partition_exists(part_name)) {
                        snprintf(out_data[cnt + tmp2].img_name, sizeof(out_data[0].img_name), "%s", read_partition_img_name(part_name));
                        snprintf(out_data[cnt + tmp2].dev_name, sizeof(out_data[0].dev_name), "/dev/mmcblk0p%d", i + 1);
                        out_data[cnt + tmp2].ab = tmp;
                        out_data[cnt + tmp2].size = 1;
                        cnt++;
                    }
                    break;
                }
            }
        }
        pclose(fp);
    }
    if (cnt != nums) {
        LOGE(TAG, "get part error.");
        goto err;
    }
    out_data[cnt + tmp2].size = 0;
    *data = out_data;
    return 0;
err:
    aos_free(out_data);
    return -1;
}

static int get_partition_info(const char *img_name, size_t img_size, size_t *out_size,
                            unsigned long *fp, int *fd, char *out_dev_name, char *out_img_path)
{
    int i;
    int rootfsab, kernelab;
    int is_uboot;

    *out_size = 0;
    *fd = -1;
    *fp = 0;
    is_uboot = 0;

    if (strcmp(img_name, IMG_NAME_UBOOT) == 0) {
        is_uboot = 1;
    }
    kernelab = check_kernel_partition();
    if (kernelab == 1) {
        kernelab = 2;
    } else {
        kernelab = 1;
    }
    rootfsab = check_rootfs_partition();
    if (rootfsab == 1) {
        rootfsab = 2;
    } else {
        rootfsab = 1;
    }
    LOGD(TAG, "kernelab:%d, rootfsab:%d", kernelab, rootfsab);
    i = 0;
    while(1) {
        if (strcmp(img_name, IMG_NAME_DIFF) == 0) {
            int ffd;
            struct statvfs vfs;
            if (statvfs("/", &vfs) < 0) {
                return -1;
            }
            *out_size = vfs.f_bavail * vfs.f_bsize;
            if (img_size >= *out_size) {
                LOGE(TAG, "the package[%d] is larger than disk space[%d].", img_size, *out_size);
                return -1;
            }
            ffd = open("/"IMG_NAME_DIFF, O_CREAT | O_RDWR | O_SYNC, 0666);
            if (ffd < 0) {
                LOGE(TAG, "open diff temp file failed.");
                return -1;
            }
            *fd = ffd;
            return 0;
        }
        if (g_partition_info[i].size == 0) {
            break;
        }
        if (strcmp(img_name, g_partition_info[i].img_name) == 0) {
            if (is_uboot == 1) {
                LOGD(TAG, "got uboot devname: %s", g_partition_info[i].dev_name);
                char *namepath = strdup_img_path(img_name);
                if (namepath == NULL) {
                    return -ENOMEM;
                }
                strncpy(out_img_path, namepath, IMG_PATH_MAX_LEN);
                out_img_path[IMG_PATH_MAX_LEN - 1] = 0;
                int ffd;
                ffd = open(namepath, O_CREAT | O_RDWR | O_SYNC, 0666);
                aos_free(namepath);
                if (ffd < 0) {
                    LOGE(TAG, "open uboot temp file failed.");
                    return -1;
                }
                *fd = ffd;
                LOGD(TAG, "@@@[%d].*fp:0x%08x", i, *fp);
                memcpy(out_dev_name, g_partition_info[i].dev_name, sizeof(g_partition_info[i].dev_name));
                // Open and size the device
                size_t memsize;
                if (FILE_SYSTEM_IS_UBI()) {
                    mtd_info_t meminfo;
                    if ((ffd = open(g_partition_info[i].char_name, O_RDONLY)) < 0) {
                        LOGE(TAG, "Open device %s failed.", g_partition_info[i].char_name);
                        return -1;
                    }
                    if (ioctl(ffd, MEMGETINFO, &meminfo) != 0) {
                        LOGE(TAG, "ioctl(MEMGETINFO) error,[fd:%d][errno:%d]", ffd, errno);
                        close(ffd);
                        return -1;
                    }
                    close(ffd);
                    memsize = meminfo.size;
                }
                if (FILE_SYSTEM_IS_EXT4()) {
                    if ((ffd = open(g_partition_info[i].char_name, O_RDONLY)) < 0) {
                        LOGE(TAG, "Open device %s failed.", g_partition_info[i].char_name);
                        return -1;
                    }
                    if (ioctl(ffd, BLKGETSIZE64, &memsize) != 0) {
                        LOGE(TAG, "ioctl(BLKGETSIZE64) error,[fd:%d][errno:%d]", ffd, errno);
                        close(ffd);
                        return -1;
                    }
                    close(ffd);
                }
                g_partition_info[i].size = memsize;
                *out_size = g_partition_info[i].size;
                LOGD(TAG, "partition size:%d", *out_size);
                return 0;
            } else {
                int ret;
                long long image_size = img_size;
                if ((strcmp(img_name, IMG_NAME_KERNEL) == 0 && g_partition_info[i].ab == kernelab)
                    || (strcmp(img_name, IMG_NAME_ROOTFS) == 0 && g_partition_info[i].ab == rootfsab)
                    || (strcmp(img_name, g_partition_info[i].img_name) == 0 && g_partition_info[i].ab == check_partition_ab(img_name))) {
                    LOGD(TAG, "got devname: %s", g_partition_info[i].dev_name);
                    LOGD(TAG, "img_size: %d", img_size);
                    int ffd = open(g_partition_info[i].dev_name, O_RDWR | O_SYNC);
                    if (ffd < 0) {
                        LOGE(TAG, "open image: %s, [%s]file failed.[errno:%d]", img_name, g_partition_info[i].dev_name, errno);
                        return -1;
                    }
                    size_t bytes;
                    if (FILE_SYSTEM_IS_UBI()) {
                        long long bytes2;
                        extern int get_ubi_info(const char *ubi_name, long long *bytes2);
                        if (get_ubi_info(g_partition_info[i].dev_name, &bytes2)) {
                            LOGE(TAG, "get %s length error.", g_partition_info[i].dev_name);
                            close(ffd);
                            return -1;
                        }
                        bytes = bytes2;
                    }
                    if (FILE_SYSTEM_IS_EXT4()) {
                        if (ioctl(ffd, BLKGETSIZE64, &bytes) != 0) {
                            LOGE(TAG, "ioctl(BLKGETSIZE64) error,[fd:%d][errno:%d]", ffd, errno);
                            close(ffd);
                            return -1;
                        }
                    }
                    g_partition_info[i].size = (int)bytes;
                    LOGD(TAG, "the %s bytes:%d", g_partition_info[i].dev_name, bytes);
                    if (image_size > bytes) {
                        LOGE(TAG, "the image_size[%d] is larger than partition_size[%d].", image_size, bytes);
                        close(ffd);
                        return -1;
                    }
                    if (FILE_SYSTEM_IS_UBI()) {
                        ret = ioctl(ffd, UBI_IOCVOLUP, &image_size);
                        if (ret < 0) {
                            LOGE(TAG, "UBI_IOCVOLUP failed, ffd:%d,[%s][ret:%d][errno:%d].", ffd, g_partition_info[i].dev_name, ret, errno);
                            close(ffd);
                            return -1;
                        }
                    }
                    *fd = ffd;
                    LOGD(TAG, "###[%d].*fd:%d, img_size:%d, image_size:%d", i, *fd, img_size, image_size);
                    *out_size = g_partition_info[i].size;
                    memcpy(out_dev_name, g_partition_info[i].dev_name, sizeof(g_partition_info[i].dev_name));
                    LOGD(TAG, "partition size:%d", *out_size);
                    return 0;
                }
            }
        }
        i++;
    }
    return -1;
}

static int set_img_info(netio_t *io, uint8_t *buffer)
{
    download_img_info_t *priv = (download_img_info_t *)io->private;
    pack_header_v2_t *header = (pack_header_v2_t *)buffer;
    pack_header_imginfo_v2_t *imginfo = header->image_info;

    LOGD(TAG, "come to set image info.");
    if (header->magic != PACK_HEAD_MAGIC) {
        LOGE(TAG, "the image header is wrong.");
        return -1;
    }
    if (header->image_count > IMG_MAX_COUNT) {
        LOGE(TAG, "the image count is overflow.");
        return -1;
    }
    priv->image_count = header->image_count;
    priv->head_size = header->head_size;
    priv->digest_type = header->digest_type;
    priv->signature_type = header->signature_type;
    memcpy(priv->md5sum, header->md5sum, sizeof(priv->md5sum));
    memcpy(priv->signature, header->signature, sizeof(priv->signature));
    LOGD(TAG, "image count is :%d", header->image_count);
    for (int i = 0; i < header->image_count; i++) {
        LOGD(TAG, "-------> %s", imginfo->img_name);
        LOGD(TAG, "offset:%d", imginfo->offset);
        LOGD(TAG, "size:%d", imginfo->size);
        memcpy(priv->img_info[i].img_name, imginfo->img_name, IMG_NAME_MAX_LEN);
        priv->img_info[i].img_offset = imginfo->offset;
        priv->img_info[i].img_size = imginfo->size;
        priv->img_info[i].fp = NULL;
        priv->img_info[i].fd = -1;
        unsigned long ffp;
        int ret = get_partition_info(priv->img_info[i].img_name, priv->img_info[i].img_size,
                                     &priv->img_info[i].partition_size, &ffp, &priv->img_info[i].fd,
                                     priv->img_info[i].dev_name, priv->img_info[i].img_path);
        if (ret < 0) {
            LOGE(TAG, "get partition info error.");
            return -1;
        }
        priv->img_info[i].fp = (FILE *)ffp;
        LOGD(TAG, "@@@priv->img_info[%d].fp:0x%08x, fd:%d", i, priv->img_info[i].fp, priv->img_info[i].fd);
        imginfo++;
    }
    return 0;
}

static int get_img_index(netio_t *io, size_t cur_offset)
{
    int i;
    size_t offsets[IMG_MAX_COUNT + 1];

    download_img_info_t *priv = (download_img_info_t *)io->private;
    LOGD(TAG, "%s, cur_offset: %d", __func__, cur_offset);
    if (cur_offset == 0) {
        return 0;
    }
    for (i = 0; i < IMG_MAX_COUNT + 1; i++) {
        offsets[i] = ~0;
    }
    for (i = 0; i < priv->image_count; i++) {
        offsets[i] = priv->img_info[i].img_offset;
        LOGD(TAG, "offsets[%d]: %d", i, offsets[i]);
    }
    for (i = 0; i < priv->image_count; i++) {
        LOGD(TAG, "offsets[%d]: %d, offsets[%d]: %d", i, offsets[i], i+1, offsets[i+1]);
        if(cur_offset >= offsets[i] && cur_offset < offsets[i+1]) {
            LOGD(TAG, "Range Num: %d", i);
            return i;
        }
    }
    LOGE(TAG, "get img index error.");
    return -1;
}

static int flash_open(netio_t *io, const char *path)
{
    io->block_size = CONFIG_FOTA_BUFFER_SIZE;

    io->private = aos_zalloc(sizeof(download_img_info_t));
    if (io->private == NULL) {
        return -ENOMEM;
    }
    if (FILE_SYSTEM_IS_EXT4()) {
        if (get_emmc_valid_partition_info(&g_partition_info) < 0) {
            aos_free(io->private);
            return -1;
        }
    } else if (FILE_SYSTEM_IS_UBI()) {
        g_partition_info = g_partition_info_ubi;
    } else {
        aos_free(io->private);
        return -1;
    }

    return 0;
}

static int flash_close(netio_t *io)
{
    int i;
    int ret = 0;
    size_t total_size;
    download_img_info_t *priv = (download_img_info_t *)io->private;

    if (FILE_SYSTEM_IS_EXT4()) {
        aos_free(g_partition_info);
    }
    // save to file first
    FILE *fp = fopen(IMGINFOFILE, "wb+");
    if (!fp) {
        LOGE(TAG, "open %s file failed.", IMGINFOFILE);
        ret = -1;
        goto out;
    }
    if (fwrite(priv, 1, sizeof(download_img_info_t), fp) < 0) {
        ret = -1;
        goto out;
    }
    fsync(fileno(fp));

    total_size = priv->head_size;
    for (i = 0; i < priv->image_count; i++) {
        total_size += priv->img_info[i].img_size;
    }
    LOGD(TAG, "flash close, total_size:%d, io->offset:%d", total_size, io->offset);

    if (io->offset == total_size && io->offset != 0) {
        LOGD(TAG, "come to close file");
        for (i = 0; i < priv->image_count; i++) {
            if (priv->img_info[i].fp) {
                fclose(priv->img_info[i].fp);
                LOGD(TAG, "close 0x%08x", priv->img_info[i].fp);
            }
            if (priv->img_info[i].fd > 0) {
                close(priv->img_info[i].fd);
                LOGD(TAG, "close %d", priv->img_info[i].fd);
            }
        }
    } else {
        for (i = 0; i < priv->image_count; i++) {
            if (priv->img_info[i].fp) {
                LOGD(TAG, "no need to close 0x%08x", priv->img_info[i].fp);
            }
            if (priv->img_info[i].fd > 0) {
                LOGD(TAG, "no need to close %d", priv->img_info[i].fd);
            }
        }        
    }
out:
    if (fp) fclose(fp);
    if (io->private) {
        aos_free(io->private);
        io->private = NULL;
    }
    return ret;
}

static int flash_read(netio_t *io, uint8_t *buffer, int length, int timeoutms)
{
    return 0;
}

static int _file_write(netio_t *io, int idx, uint8_t *buffer, int length)
{
    int ret = -1;
    FILE *fp;
    int fd;
    download_img_info_t *priv = (download_img_info_t *)io->private;

    fp = priv->img_info[idx].fp;
    fd = priv->img_info[idx].fd;
    LOGD(TAG, "_file write fp: 0x%08x, fd: %d", fp, fd);

    if (fp) {
        ret = fwrite(buffer, sizeof(uint8_t), length, fp);
    }
    if (fd >= 0) {
        ret = write(fd, buffer, length);
    }

    if (ret < 0) {
        LOGE(TAG, "[%s, %d]write %d bytes failed, [fp:0x%08x, fd:%d], ret:%d, errno:%d",
             __func__, __LINE__, length, fp, fd, ret, errno);
        return -1;
    }
    if (fp) fflush(fp);
    return ret;
}

static int download_img_info_init(netio_t *io, uint8_t *buffer, int length, int buffer_save)
{
    int headsize = 0;
    pack_header_v2_t *header;

    header = (pack_header_v2_t *)buffer;
    download_img_info_t *priv = (download_img_info_t *)io->private;

    if (length < sizeof(pack_header_v2_t)) {
        LOGE(TAG, "the first size %d is less than %d", length, sizeof(pack_header_v2_t));
        return -1;
    }
    if (buffer_save) {
        FILE *headerfp = fopen(IMGHEADERPATH, "wb+");
        if (!headerfp) {
            LOGE(TAG, "create %s file failed.", IMGHEADERPATH);
            return -1;
        }
        if (fwrite(buffer, 1, sizeof(pack_header_v2_t), headerfp) < 0) {
            LOGE(TAG, "write %s failed.", IMGHEADERPATH);
            fclose(headerfp);
            return -1;
        }
        fsync(fileno(headerfp));
        fclose(headerfp);
    }
    LOGD(TAG, "head_version:%d, head_size:%d, checksum:0x%08x, count:%d, digest:%d, signature:%d",
                header->head_version, header->head_size, header->head_checksum, header->image_count,
                header->digest_type, header->signature_type);
    headsize = header->head_size;
    uint8_t *tempbuf = aos_zalloc(headsize);
    if (!tempbuf) {
        return -ENOMEM;
    }
    memcpy(tempbuf, buffer, headsize);
    ((pack_header_v2_t *)tempbuf)->head_checksum = 0;
    memset(((pack_header_v2_t *)tempbuf)->signature, 0, sizeof(((pack_header_v2_t *)tempbuf)->signature));
    uint32_t cksum = get_checksum((uint8_t *)tempbuf, headsize);
    aos_free(tempbuf);
    if (cksum != header->head_checksum) {
        LOGE(TAG, "the header checksum error.[0x%08x, 0x%08x]", header->head_checksum, cksum);
        return -1;
    }
    FILE *imginfofp = fopen(IMGINFOFILE, "wb+");
    if (!imginfofp) {
        LOGE(TAG, "create %s file failed.", IMGINFOFILE);
        return -1;
    }
    if (set_img_info(io, buffer) < 0) {
        LOGE(TAG, "set imageinfo failed.");
        return -1;
    }
    if (fwrite(priv, 1, sizeof(download_img_info_t), imginfofp) < 0) {
        LOGE(TAG, "write %s file failed.", IMGINFOFILE);
        fclose(imginfofp);
        return -1;
    }
    fsync(fileno(imginfofp));
    fclose(imginfofp);

    return 0;
}

static int download_img_info_init_from_file(netio_t *io)
{
    int ret;
    int fd;
    uint8_t *buffer;
    uint64_t length;

    fd = open(IMGHEADERPATH, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    buffer = NULL;
    length = lseek(fd, 0, SEEK_END);
    if (length <= 0) {
        ret = -1;
        goto out;
    }
    buffer = aos_zalloc(sizeof(download_img_info_t));
    if (buffer == NULL) {
        ret = -ENOMEM;
        goto out;
    }
    lseek(fd, 0, SEEK_SET);
    if (read(fd, buffer, length) != length) {
        ret = -1;
        goto out;
    }
    ret = download_img_info_init(io, buffer, length, 0);
    if (ret < 0) {
        ret = -1;
        goto out;
    }
    ret = 0;
out:
    if (fd >= 0) {
        close(fd);
    }
    if (buffer) {
        aos_free(buffer);
    }
    return ret;
}

static int flash_write(netio_t *io, uint8_t *buffer, int length, int timeoutms)
{
    int headsize = 0;
    pack_header_v2_t *header;
    download_img_info_t *priv = (download_img_info_t *)io->private;

    header = (pack_header_v2_t *)buffer;
    LOGD(TAG, "flash write, total: %d offset: %d len: %d", io->size, io->offset, length);

    if (io->offset == 0) {
        if (header->magic == PACK_HEAD_MAGIC) {
            LOGD(TAG, "i am the pack image.");
            int ret = download_img_info_init(io, buffer, length, 1);
            if (ret < 0) {
                return ret;
            }
            headsize = header->head_size;
            LOGD(TAG, "parse packed image ok.");
        } else {
            LOGE(TAG, "the image is not a pack image.");
            return -1;
        }
    }
    int idx = get_img_index(io, io->offset);
    if (idx < 0) {
        LOGE(TAG, "flash write error.");
        return -1;
    }
    LOGD(TAG, "idx:%d, priv->img_info[%d].img_offset:%d", idx, idx, priv->img_info[idx].img_offset);
    if (priv->img_info[idx].partition_size - (io->offset - priv->img_info[idx].img_offset) < length) {
        length = priv->img_info[idx].partition_size - (io->offset - priv->img_info[idx].img_offset);
    }
    int real_to_write_len = length - headsize;
    if (priv->img_info[idx].write_size + real_to_write_len > priv->img_info[idx].img_size) {
        int leftsize = priv->img_info[idx].img_size - priv->img_info[idx].write_size;
        if (_file_write(io, idx, &buffer[headsize], leftsize) < 0) {
            LOGE(TAG, "write leftsize %d bytes failed", leftsize);
            return -1;
        }
        LOGD(TAG, "write leftsize %d bytes ok", leftsize);
        priv->img_info[idx].write_size += leftsize;
        int remainsize = real_to_write_len - leftsize;
        if (priv->img_info[idx + 1].fp || priv->img_info[idx + 1].fd >= 0) {
            if (_file_write(io, idx + 1, &buffer[headsize + leftsize], remainsize) < 0) {
                LOGE(TAG, "write remainsize %d bytes failed", remainsize);
                return -1;
            }
            LOGD(TAG, "write remainsize %d bytes ok", remainsize);
            priv->img_info[idx + 1].write_size += remainsize;
        }
    } else {
        if (_file_write(io, idx, &buffer[headsize], real_to_write_len) < 0) {
            LOGE(TAG, "write real_to_write_len %d bytes failed", real_to_write_len);
            return -1;
        }
        LOGD(TAG, "write real_to_write_len %d bytes ok", real_to_write_len);
        priv->img_info[idx].write_size += real_to_write_len;
    }

    io->offset += length;
    return length;
}

static int flash_seek(netio_t *io, size_t offset, int whence)
{
    int idx;
    download_img_info_t *priv = (download_img_info_t *)io->private; 
    LOGD(TAG, "flash seek %d", offset);

    if (FILE_SYSTEM_IS_EXT4()) {
        if (offset && priv->image_count <= 0) {
            if (download_img_info_init_from_file(io) < 0) {
                return -1;
            }
        }
        idx = get_img_index(io, offset);
        if (idx < 0) {
            LOGE(TAG, "flash seek error.");
            return -1;
        }
        switch (whence) {
            case SEEK_SET:
                io->offset = offset;
                if (priv->img_info[idx].fp)
                    fseek(priv->img_info[idx].fp, (long)offset - priv->img_info[idx].img_offset, 0);
                if (priv->img_info[idx].fd >= 0)
                    lseek(priv->img_info[idx].fd, (long)offset - priv->img_info[idx].img_offset, 0);
                priv->img_info[idx].write_size = offset - priv->img_info[idx].img_offset;
                return 0;
        }
        return -1;
    }
#ifdef UBI_NOT_SUPPORT_INTERRUPTED_UPDATE
    int ret = 0;
    FILE *fp = NULL;
    // because of UBI not support seek.
    io->offset = offset;
    if (offset != 0) {
        fp = fopen(IMGINFOFILE, "rb+");
        if (fp) {
            if (fread(priv, 1, sizeof(download_img_info_t), fp) < sizeof(download_img_info_t)) {
                LOGE(TAG, "read %s failed.", IMGINFOFILE);
                ret = -1;
                goto out;
            }
        } else {
            LOGE(TAG, "download some data, but there is no %s file found.", IMGINFOFILE);
            if (aos_kv_setint(KV_FOTA_OFFSET, 0) < 0) {
                ret = -1;
                goto out;
            }
        }
    }
out:
    if (fp) fclose(fp);
    return ret;
#else
    idx = get_img_index(io, offset);
    if (idx < 0) {
        LOGE(TAG, "flash seek error.");
        return -1;
    }
    switch (whence) {
        case SEEK_SET:
            io->offset = offset;
            if (priv->img_info[idx].fp)
                fseek(priv->img_info[idx].fp, (long)offset - priv->img_info[idx].img_offset, 0);
            if (priv->img_info[idx].fd >= 0)
                lseek(priv->img_info[idx].fd, (long)offset - priv->img_info[idx].img_offset, 0);
            priv->img_info[idx].write_size = offset - priv->img_info[idx].img_offset;
            return 0;
    }
#endif
    return -1;
}

const netio_cls_t flash2 = {
    .name = "flash2",
    .open = flash_open,
    .close = flash_close,
    .write = flash_write,
    .read = flash_read,
    .seek = flash_seek,
};

int netio_register_flash2(void)
{
    return netio_register(&flash2);
}
