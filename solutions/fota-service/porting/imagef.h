/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include <stdint.h>

#ifndef __IMAGE_F_H__
#define __IMAGE_F_H__

#define IMG_NAME_MAX_LEN 16
#define IMG_PATH_MAX_LEN 24
#define IMG_MAX_COUNT 8
#define DEV_NAME_MAX_LEN 16

typedef enum {
    DIGEST_HASH_NONE = 0,
    DIGEST_HASH_SHA1 = 1,
    DIGEST_HASH_MD5  = 2,
    DIGEST_HASH_SHA224  = 3,
    DIGEST_HASH_SHA256  = 4,
    DIGEST_HASH_SHA384  = 5,
    DIGEST_HASH_SHA512  = 6,
    DIGEST_HASH_SM3     = 7,
    DIGEST_HASH_TYPE_END
} digest_sch_e;

typedef enum {
    SIGNATURE_NONE      = 0,
    SIGNATURE_RSA_1024  = 1,
    SIGNATURE_RSA_2048  = 2,
    SIGNATURE_ECC_256   = 3,
    SIGNATURE_ECC_160   = 4,
    SIGNATURE_SM2       = 5,
    SIGNATURE_AES_128_CCM = 10,
    SIGNATURE_TYPE_END
} signature_sch_e;

typedef struct {
    char img_name[IMG_NAME_MAX_LEN];
    uint32_t offset;
    uint32_t size;
} pack_header_imginfo_v2_t;

typedef struct {
#define PACK_IMG_MAX_COUNT 15U
#define PACK_HEAD_MAGIC 0x4B434150
    uint32_t      magic;          // "PACK"  0x4B434150
    uint16_t      head_version;   // 2
    uint16_t      head_size;      // length of header
    uint32_t      head_checksum;  // the checksum for header, fill 0 when calculate checksum
    uint32_t      image_count;    // image count to pack
    unsigned char md5sum[16];     // the md5sum for md5sum(image1+image2+image3...)
    uint16_t      digest_type;    // the digest type
    uint16_t      signature_type; // the signature type
    unsigned char signature[512]; // the signature for header + image, fill 0 when calculate checksum or calculate signature
    uint32_t      rsv[29];        // reverse
    pack_header_imginfo_v2_t image_info[PACK_IMG_MAX_COUNT]; // 24*15=360B
} pack_header_v2_t; // 1024Bytes

typedef struct {
    uint32_t image_count;
    size_t head_size;
    unsigned char md5sum[16];
    uint16_t digest_type;
    uint16_t signature_type;
    unsigned char signature[512];
    struct {
        FILE *fp;
        int fd;
        char img_name[IMG_NAME_MAX_LEN];
        char img_path[IMG_PATH_MAX_LEN];
        char dev_name[DEV_NAME_MAX_LEN];
        size_t img_offset;
        size_t img_size;
        size_t partition_size;
        size_t write_size;
        size_t read_size;
    } img_info[IMG_MAX_COUNT];
} download_img_info_t;

#define IMG_NAME_UBOOT "uboot"
#define IMG_NAME_KERNEL "kernel"
#define IMG_NAME_ROOTFS "rootfs"
#define IMG_NAME_TF "tf"
#define IMG_NAME_TEE "tee"
#define IMG_NAME_TFSTASH "tfstash"
#define IMG_NAME_TEESTASH "teestash"
#define IMG_NAME_DIFF "diff"
#define IMGINFOFILE "/fotaimgsinfo.bin"     // save download_img_info_t
#define IMGHEADERPATH "/fotaimgsheader.bin" // save pack_header_v2_t, because of signature verify need header raw data.

int get_file_size(FILE *fp, int fd);
uint32_t get_checksum(uint8_t *data, uint32_t length);
char *strdup_img_path(const char *img_name);
int check_kernel_partition(void);
int check_rootfs_partition(void);
int check_partition_ab(const char *name);
int get_rootfs_file_system_type(void);
#define FILE_SYSTEM_IS_UBI() (get_rootfs_file_system_type() == 1)
#define FILE_SYSTEM_IS_EXT4() (get_rootfs_file_system_type() == 2)
int mbed_sha1_rsa_verify(uint8_t *pub_key, uint32_t key_size, uint8_t *hash, uint8_t *sig);
int mbed_sha256_rsa_verify(uint8_t *pub_key, uint32_t key_size, uint8_t *hash, uint8_t *sig);
int set_rollback_env_param(int limit_c);
int set_fota_success_param(void);
int set_ubivol_cmd(const char* vol_name, char *o_cmd, int len);
static inline int check_partition_exists(const char *name)
{
    const char *p[] = {"uboot", "boot", "root",
        IMG_NAME_TF, IMG_NAME_TEE, IMG_NAME_TFSTASH, IMG_NAME_TEESTASH};
    int i;

    for (i = 0; i < sizeof(p) / sizeof(p[0]); i++) {
        if (!strcmp(name, p[i])) {
            return 1;
        }
    }
    return 0;
}

static inline const char *read_partition_img_name(const char *name)
{
    const char *p[] = {"uboot", "boot", "root",
        IMG_NAME_TF, IMG_NAME_TEE, IMG_NAME_TFSTASH, IMG_NAME_TEESTASH};
    const char *p2[] = {IMG_NAME_UBOOT, IMG_NAME_KERNEL, IMG_NAME_ROOTFS,
        IMG_NAME_TF, IMG_NAME_TEE, IMG_NAME_TFSTASH, IMG_NAME_TEESTASH};
    int i;

    for (i = 0; i < sizeof(p) / sizeof(p[0]); i++) {
        if (!strcmp(name, p[i])) {
            return p2[i];
        }
    }
    return NULL;
}

#endif