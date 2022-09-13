/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <mbedtls/md5.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <aos/kernel.h>
#include "yoc/fota.h"
#include <ulog/ulog.h>
#include "imagef.h"

#define TAG "fotav"

static uint8_t g_pubkey_rsa[] = {
    0xeb, 0xe3, 0xa4, 0x41, 0x10, 0xd6, 0x25, 0x98, 0xc2, 0x27, 0x8b, 0x36, 0xac, 0xfa, 0xc3, 0x86,
    0x01, 0x21, 0x16, 0x64, 0x6f, 0xf2, 0x37, 0x72, 0xf4, 0xc0, 0x60, 0xab, 0x38, 0x60, 0x47, 0x7a,
    0x94, 0x7f, 0x80, 0x48, 0xa8, 0xeb, 0xa5, 0xf7, 0x1b, 0x9f, 0xf2, 0xc2, 0xf9, 0x39, 0x08, 0xbc,
    0xa9, 0x3e, 0x3c, 0x0c, 0x52, 0x15, 0xb3, 0x57, 0x81, 0x5e, 0x02, 0xf2, 0xd7, 0x7e, 0x04, 0x4c,
    0x6d, 0x93, 0xc4, 0x5d, 0xa3, 0x97, 0x17, 0xa6, 0x83, 0xa5, 0x9c, 0xc4, 0x91, 0xcc, 0x2d, 0x78,
    0xeb, 0x64, 0xbf, 0x05, 0x4d, 0x0b, 0xed, 0x09, 0x4f, 0xd4, 0x2d, 0x46, 0x2d, 0xf0, 0xfc, 0xd0,
    0x03, 0xc0, 0xb7, 0x5c, 0x19, 0x6a, 0x87, 0x11, 0xff, 0xf8, 0xa4, 0x6a, 0x55, 0xa2, 0xf6, 0x25,
    0xb9, 0x00, 0x96, 0xaf, 0x2c, 0x7f, 0x15, 0xb9, 0xe3, 0xe3, 0x24, 0x3d, 0xc3, 0xa7, 0xa4, 0x37,
};

// static uint8_t g_pubkey_rsa[] = {
//     0xe5, 0x3f, 0x3e, 0xa4, 0x54, 0xab, 0x56, 0xea, 0x96, 0x6c, 0x8c, 0x00, 0xc8, 0x6c, 0x67, 0x1c, 
//     0xa8, 0x7d, 0x7e, 0xd6, 0x43, 0xaf, 0x38, 0x59, 0xbb, 0xc0, 0x10, 0x6b, 0x90, 0xf9, 0xf6, 0x96,
//     0x3b, 0xcb, 0x46, 0xdf, 0xce, 0xad, 0xbd, 0x81, 0x00, 0x81, 0x6b, 0xa6, 0x50, 0xae, 0xa7, 0x2b,
//     0xd5, 0x0f, 0x11, 0x81, 0xbd, 0x6b, 0xff, 0xcb, 0x10, 0xd2, 0x73, 0xc5, 0xaa, 0x09, 0x5f, 0x13,
//     0xb5, 0xde, 0x89, 0x1a, 0xa8, 0x83, 0x2a, 0x79, 0x18, 0x1a, 0x5a, 0xa3, 0x1c, 0x69, 0xe7, 0x76,
//     0x3f, 0x9f, 0x78, 0x02, 0x66, 0x3e, 0x26, 0x01, 0x99, 0x1c, 0xb9, 0x75, 0x24, 0x45, 0x45, 0xc5,
//     0x1e, 0x2f, 0x8e, 0xdc, 0x4b, 0xfe, 0x70, 0x67, 0x72, 0xbc, 0xaa, 0x6d, 0xb7, 0x6a, 0x48, 0xf6,
//     0xb1, 0xa4, 0x91, 0x04, 0xbf, 0xa6, 0xe8, 0xa1, 0x4f, 0xf2, 0x4a, 0x3a, 0x14, 0x6f, 0x38, 0x0f,
//     0xb1, 0x69, 0x13, 0xbb, 0xd3, 0xc4, 0xc4, 0x4a, 0x0d, 0x4d, 0x1f, 0x8c, 0x41, 0x11, 0xd7, 0xfc,
//     0xff, 0x85, 0x0a, 0x7c, 0xfb, 0x74, 0x8f, 0x01, 0x37, 0x40, 0xec, 0x5d, 0x31, 0x6b, 0x9d, 0x6c,
//     0xd4, 0xfd, 0xd2, 0xcc, 0xd5, 0x34, 0xd4, 0xb9, 0x93, 0xf3, 0xc7, 0xba, 0x3a, 0xfb, 0xb6, 0x2b,
//     0x5e, 0xc1, 0x51, 0xce, 0xb6, 0xec, 0x5c, 0x80, 0x59, 0x89, 0x3c, 0x32, 0x2a, 0x51, 0xce, 0xc5,
//     0x10, 0x2e, 0x71, 0xd7, 0x18, 0x86, 0x58, 0xfd, 0xc0, 0xc7, 0xaa, 0x03, 0x25, 0x7d, 0x5e, 0x98,
//     0x4b, 0x24, 0x9a, 0x50, 0x83, 0x77, 0x59, 0x25, 0xb0, 0x4d, 0x28, 0xe4, 0x0a, 0x12, 0xed, 0x5b,
//     0x16, 0xac, 0x17, 0x33, 0xb6, 0xe6, 0x82, 0x5c, 0x0f, 0xb4, 0x51, 0x14, 0x7d, 0xff, 0x98, 0xaf,
//     0x04, 0x70, 0xd5, 0xd4, 0x64, 0xb9, 0x33, 0x81, 0x55, 0xc3, 0xad, 0x0d, 0xbb, 0x3d, 0x72, 0xb1,
// };

static int _file_read(int idx, download_img_info_t *dl_img_info, void *buffer, int length)
{
    int ret = -1;
    FILE *fp;
    int fd;

    fp = dl_img_info->img_info[idx].fp;
    fd = dl_img_info->img_info[idx].fd;

    if (fp) {
        ret = fread(buffer, sizeof(uint8_t), length, fp);
    }
    if (fd > 0) {
        ret = read(fd, buffer, length);
    }
    if (ret < 0 || ret != length) {
        LOGE(TAG, "[%s, %d]fp:0x%08x, fd:%d, length:%d, ret:%d, errno:%d", __func__, __LINE__, fp, fd, length, ret, errno);
        return -1;
    }
    return ret;
}

int fota_data_verify(void)
{
    int len;
    FILE *fp = NULL;
    download_img_info_t dl_img_info;
    uint8_t temp_buffer[4096] __attribute__((aligned(4)));

    LOGD(TAG, "come to image verify.");

    fp = fopen(IMGINFOFILE, "rb+");
    if (fp != NULL) {
        len = get_file_size(fp, -1);
        if (len < sizeof(download_img_info_t)) {
            LOGE(TAG, "The %s file length is wrong.", IMGINFOFILE);
            goto errout;
        }
        if (fread(&dl_img_info, 1, sizeof(download_img_info_t), fp) < sizeof(download_img_info_t)) {
            goto errout;
        }
        LOGD(TAG, "dl_img_info.image_count:%d", dl_img_info.image_count);
        for (int i = 0; i < dl_img_info.image_count; i++) {
            LOGD(TAG, "%s, size: %d", dl_img_info.img_info[i].img_name, dl_img_info.img_info[i].img_size);
        }
        LOGD(TAG, "dl_img_info.digest_type:%d", dl_img_info.digest_type);
        if (dl_img_info.digest_type > 0) {
            uint8_t hash_out[128];

            LOGD(TAG, "come to verify signature.");
            if (dl_img_info.digest_type >= DIGEST_HASH_TYPE_END) {
                LOGE(TAG, "the digest type %d is error", dl_img_info.digest_type);
                goto errout;
            }
            FILE *headerfp = fopen(IMGHEADERPATH, "rb+");
            if (!headerfp) {
                LOGE(TAG, "can't find %s.", IMGHEADERPATH);
                goto errout;
            }
            if (fread(temp_buffer, 1, sizeof(pack_header_v2_t), headerfp) < sizeof(pack_header_v2_t)) {
                LOGE(TAG, "read %s error.", IMGHEADERPATH);
                fclose(headerfp);
                goto errout;
            }
            fclose(headerfp);
            if (dl_img_info.digest_type == DIGEST_HASH_SHA1) {
                mbedtls_sha1_context ctx;
                mbedtls_sha1_init(&ctx);
                mbedtls_sha1_starts(&ctx);
                // SHA header first
                memset(((pack_header_v2_t *)temp_buffer)->signature, 0, sizeof(((pack_header_v2_t *)temp_buffer)->signature));
                mbedtls_sha1_update(&ctx, temp_buffer, sizeof(pack_header_v2_t));

                for (int i = 0; i < dl_img_info.image_count; i++) {
                    int image_size = dl_img_info.img_info[i].img_size;
                    int fpsize = get_file_size(dl_img_info.img_info[i].fp, dl_img_info.img_info[i].fd);
                    LOGD(TAG, "### [fpsize:%d, image_size:%d]", fpsize, image_size);
                    if (dl_img_info.img_info[i].fp) {
                        if (fpsize != image_size) {
                            LOGE(TAG, "the imagesize is not matched.[fpsize:%d, image_size:%d]", fpsize, image_size);
                            goto errout;
                        }
                    } else if (dl_img_info.img_info[i].fd > 0) {
                        // UBI Volume
                    }
                    if (image_size > sizeof(temp_buffer)) {
                        if (_file_read(i, &dl_img_info, temp_buffer, sizeof(temp_buffer)) < 0) {
                            goto errout;
                        }
                        mbedtls_sha1_update(&ctx, temp_buffer, sizeof(temp_buffer));
                        image_size -= sizeof(temp_buffer);

                        while (image_size > sizeof(temp_buffer)) {
                            if (_file_read(i, &dl_img_info, temp_buffer, sizeof(temp_buffer)) < 0) {
                                goto errout;
                            }
                            mbedtls_sha1_update(&ctx, temp_buffer, sizeof(temp_buffer));
                            image_size -= sizeof(temp_buffer);
                        }
                        if (_file_read(i, &dl_img_info, temp_buffer, image_size) < 0) {
                            goto errout;
                        }
                        mbedtls_sha1_update(&ctx, temp_buffer, image_size);
                    } else {
                        if (_file_read(i, &dl_img_info, temp_buffer, image_size) < 0) {
                            goto errout;
                        }
                        mbedtls_sha1_update(&ctx, temp_buffer, image_size);
                    }
                }
                mbedtls_sha1_finish(&ctx, hash_out);
                mbedtls_sha1_free(&ctx);
                if (mbed_sha1_rsa_verify(g_pubkey_rsa, sizeof(g_pubkey_rsa), hash_out, dl_img_info.signature) != 0) {
                    LOGE(TAG, "sha1 rsa verify failed.");
                    goto errout;
                }
            } else if (dl_img_info.digest_type == DIGEST_HASH_SHA256) {
                mbedtls_sha256_context ctx;
                mbedtls_sha256_init(&ctx);
                mbedtls_sha256_starts(&ctx, 0);
                // SHA header first
                memset(((pack_header_v2_t *)temp_buffer)->signature, 0, sizeof(((pack_header_v2_t *)temp_buffer)->signature));
                mbedtls_sha256_update(&ctx, temp_buffer, sizeof(pack_header_v2_t));

                for (int i = 0; i < dl_img_info.image_count; i++) {
                    int image_size = dl_img_info.img_info[i].img_size;
                    int fpsize = get_file_size(dl_img_info.img_info[i].fp, dl_img_info.img_info[i].fd);
                    LOGD(TAG, "### [fpsize:%d, image_size:%d]", fpsize, image_size);
                    if (dl_img_info.img_info[i].fp) {
                        if (fpsize != image_size) {
                            LOGE(TAG, "the imagesize is not matched.[fpsize:%d, image_size:%d]", fpsize, image_size);
                            goto errout;
                        }
                    } else if (dl_img_info.img_info[i].fd > 0) {
                        // UBI Volume
                    }
                    if (image_size > sizeof(temp_buffer)) {
                        if (_file_read(i, &dl_img_info, temp_buffer, sizeof(temp_buffer)) < 0) {
                            goto errout;
                        }
                        mbedtls_sha256_update(&ctx, temp_buffer, sizeof(temp_buffer));
                        image_size -= sizeof(temp_buffer);

                        while (image_size > sizeof(temp_buffer)) {
                            if (_file_read(i, &dl_img_info, temp_buffer, sizeof(temp_buffer)) < 0) {
                                goto errout;
                            }
                            mbedtls_sha256_update(&ctx, temp_buffer, sizeof(temp_buffer));
                            image_size -= sizeof(temp_buffer);
                        }
                        if (_file_read(i, &dl_img_info, temp_buffer, image_size) < 0) {
                            goto errout;
                        }
                        mbedtls_sha256_update(&ctx, temp_buffer, image_size);
                    } else {
                        if (_file_read(i, &dl_img_info, temp_buffer, image_size) < 0) {
                            goto errout;
                        }
                        mbedtls_sha256_update(&ctx, temp_buffer, image_size);
                    }
                }
                mbedtls_sha256_finish(&ctx, hash_out);
                mbedtls_sha256_free(&ctx);
                if (mbed_sha256_rsa_verify(g_pubkey_rsa, sizeof(g_pubkey_rsa), hash_out, dl_img_info.signature) != 0) {
                    LOGE(TAG, "sha256 rsa verify failed.");
                    goto errout;
                }
            } else {
                LOGE(TAG, "digest type e[%d]", dl_img_info.digest_type);
                goto errout;
            }
        } else {
            uint8_t md5_out[16];
            mbedtls_md5_context md5;
            LOGD(TAG, "come to MD5 verify.");
            mbedtls_md5_init(&md5);
            mbedtls_md5_starts(&md5);
            for (int i = 0; i < dl_img_info.image_count; i++) {
                int image_size = dl_img_info.img_info[i].img_size;
                int fpsize = get_file_size(dl_img_info.img_info[i].fp, dl_img_info.img_info[i].fd);
                LOGD(TAG, "### [fpsize:%d, image_size:%d]", fpsize, image_size);
                if (dl_img_info.img_info[i].fp) {
                    if (fpsize != image_size) {
                        LOGE(TAG, "the imagesize is not matched.[fpsize:%d, image_size:%d]", fpsize, image_size);
                        goto errout;
                    }
                } else if (dl_img_info.img_info[i].fd > 0) {
                        // UBI Volume
                }
                if (image_size > sizeof(temp_buffer)) {
                    if (_file_read(i, &dl_img_info, temp_buffer, sizeof(temp_buffer)) < 0) {
                        goto errout;
                    }
                    mbedtls_md5_update(&md5, temp_buffer, sizeof(temp_buffer));
                    image_size -= sizeof(temp_buffer);

                    while (image_size > sizeof(temp_buffer)) {
                        if (_file_read(i, &dl_img_info, temp_buffer, sizeof(temp_buffer)) < 0) {
                            goto errout;
                        }
                        mbedtls_md5_update(&md5, temp_buffer, sizeof(temp_buffer));
                        image_size -= sizeof(temp_buffer);
                    }
                    if (_file_read(i, &dl_img_info, temp_buffer, image_size) < 0) {
                        goto errout;
                    }
                    mbedtls_md5_update(&md5, temp_buffer, image_size);
                } else {
                    if (_file_read(i, &dl_img_info, temp_buffer, image_size) < 0) {
                        goto errout;
                    }
                    mbedtls_md5_update(&md5, temp_buffer, image_size);
                }
            }
            mbedtls_md5_finish(&md5, md5_out);
            mbedtls_md5_free(&md5);
            if (memcmp(dl_img_info.md5sum, md5_out, 16) != 0) {
                printf("origin md5sum:\n");
                for (int kk = 0; kk < 16; kk++) {
                    printf("0x%02x ", dl_img_info.md5sum[kk]);
                }
                printf("\r\n");
                printf("calculate md5sum:\n");
                for (int kk = 0; kk < 16; kk++) {
                    printf("0x%02x ", md5_out[kk]);
                }
                printf("\r\n");
                LOGE(TAG, "image md5sum verify failed.");
                goto errout;
            }
        }
        LOGD(TAG, "image verify ok.");
        fclose(fp);
        return 0;
    }
errout:
    LOGD(TAG, "image verify error.");
    if (fp) {
        fclose(fp);
        for (int i = 0; i < dl_img_info.image_count; i++) {
            if (dl_img_info.img_info[i].fp) {
                fclose(dl_img_info.img_info[i].fp);
                LOGD(TAG, "close 0x%08x", dl_img_info.img_info[i].fp);
            }
            if (dl_img_info.img_info[i].fd > 0) {
                close(dl_img_info.img_info[i].fd);
                LOGD(TAG, "close %d",dl_img_info.img_info[i].fd);
            }
        }
    }
    return -1;
}
