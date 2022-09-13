/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <mbedtls/rsa.h>
#include <aos/kernel.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "imagef.h"

int get_file_size(FILE *fp, int fd)
{
    int file_len = 0;
    if (fp) {
        fseek(fp, 0, SEEK_END);
        file_len = ftell(fp);
        fseek(fp, 0, SEEK_SET);
    }
    if (fd >= 0) {
        file_len = lseek(fd, 0L, SEEK_END);
        lseek(fd, 0L, SEEK_SET);
        // FIXME: UBI return the partition length, not the file content size
    }
    // LOGD(TAG, "file_len: %d", file_len);
    return file_len;
}

uint32_t get_checksum(uint8_t *data, uint32_t length)
{
    uint32_t cksum = 0;
    for (int i = 0; i < length; i++) {
        // if (i % 16 == 0) printf("\n");
        // printf("0x%02x ", *data);
        cksum += *data++;
    }
    // printf("\n");
    return cksum;
}

char *strdup_img_path(const char *img_name)
{
    if (img_name == NULL) {
        return NULL;
    }
    char *namepath = aos_zalloc(IMG_PATH_MAX_LEN);
    if (namepath == NULL) {
        return NULL;
    }
    snprintf(namepath, IMG_PATH_MAX_LEN, "/fota_%s.bin", img_name);
    return namepath;
}

int check_kernel_partition(void)
{
    FILE *fp;
    char buf[128] = {0};

    if ((fp = popen("fw_printenv -n boot_partition", "r")) == NULL) {
        return -1;
    }

    if (NULL == fgets(buf, sizeof(buf), fp)) //read line by line
    {
        pclose(fp);
        return -1;
    }

    pclose(fp);

    if (strstr(buf, "bootA"))
    {
        return 1;
    }
    else if (strstr(buf, "bootB"))
    {
        return 2;
    }

    return 0;
}

int check_rootfs_partition(void)
{
    FILE *fp;
    char buf[128] = {0};

    if ((fp = popen("fw_printenv -n root_partition", "r")) == NULL) {
        return -1;
    }

    if (NULL == fgets(buf, sizeof(buf), fp)) //read line by line
    {
        pclose(fp);
        return -1;
    }

    pclose(fp);

    if (strstr(buf, "rootfsA"))
    {
        return 1;
    }
    else if (strstr(buf, "rootfsB"))
    {
        return 2;
    }

    return 0;
}

int check_partition_ab(const char *name)
{
    FILE *fp;
    char buf[128] = {0};

    snprintf(buf, sizeof(buf) - 1, "fw_printenv -n %s_partition", name);
    if ((fp = popen(buf, "r")) == NULL) {
        return -1;
    }

    if (NULL == fgets(buf, sizeof(buf), fp)) //read line by line
    {
        pclose(fp);
        return -1;
    }

    pclose(fp);

    if (strstr(buf, "A"))
    {
        return 1;
    }
    else if (strstr(buf, "B"))
    {
        return 2;
    }

    return 0;
}

int get_rootfs_file_system_type(void)
{
    static int type = 0;
    FILE *fp;
    char buf[128];

    if (type) {
        return type;
    }
    fp = fopen("/proc/cmdline", "r");
    if (fp == NULL) {
        return -1;
    }

    if (fgets(buf, sizeof(buf), fp) == NULL) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (strstr(buf, "rootfstype=ext4")) {
        type = 2;
    } else {
        type = 1;
    }
    return type;
}

int mbed_sha1_rsa_verify(uint8_t *pub_key, uint32_t key_size, uint8_t *hash, uint8_t *sig)
{
    int ret;
    mbedtls_rsa_context rsa;

    // for (int i = 0; i < 128; i++) {
    //     if (i % 16 == 0) printf("\n");
    //     printf("0x%02x ", sig[i]);
    // }
    // printf("\n");

    mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);

    mbedtls_mpi_read_binary(&rsa.N, pub_key, key_size);
    rsa.len = mbedtls_mpi_size(&rsa.N);
    mbedtls_mpi_read_string(&rsa.E, 16, "10001");

    ret = mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA1, 0, hash, sig);
    if (ret != 0) {
        printf("rsa verify failed!!, ret:-0x%04x\n", ret);
        goto exit;
    }

exit:
    mbedtls_rsa_free(&rsa);
    return ret;
}

int mbed_sha256_rsa_verify(uint8_t *pub_key, uint32_t key_size, uint8_t *hash, uint8_t *sig)
{
    int ret;
    mbedtls_rsa_context rsa;

    mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V15, 0);

    mbedtls_mpi_read_binary(&rsa.N, pub_key, key_size);
    rsa.len = mbedtls_mpi_size(&rsa.N);
    mbedtls_mpi_read_string(&rsa.E, 16, "10001");

    ret = mbedtls_rsa_pkcs1_verify(&rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, MBEDTLS_MD_SHA256, 0, hash, sig);
    if (ret != 0) {
        printf("rsa verify failed!!, ret:-0x%04x\n", ret);
        goto exit;
    }

exit:
    mbedtls_rsa_free(&rsa);
    return ret;
}

int set_rollback_env_param(int limit_c)
{
    int ret;
    char cmd[64] = {0};

    snprintf(cmd, sizeof(cmd), "fw_setenv bootlimit %d", limit_c);
    ret = system(cmd);
    ret |= system("fw_setenv bootcount 0");
    ret |= system("fw_setenv upgrade_available 1");

    return ret;
}

int fw_setenv_run_script(const char *cmd)
{
    FILE *fp;
    
    fp = popen("fw_setenv -s -", "w");
    if(fp != NULL) {
        if (fwrite(cmd, strlen(cmd), 1, fp) != strlen(cmd)) {
            pclose(fp);
            return -1;
        }
        pclose(fp);
        return 0;
    }
    return -1;
}

// return value: 0--do not need upgrade; 1--upgrade failed, rollback already; 2--upgrade ok.
int set_fota_success_param(void)
{
    FILE *fp;
    int var = 0;
    int ret = 0;
    int bootlimit = 0, bootcount = 0;
    char buf[64] = {0}, cmd[128];

    if ((fp = popen("fw_printenv -n upgrade_available", "r")) == NULL) {
        return -1;
    }

    *buf = 0;
    fgets(buf, sizeof(buf), fp);        //read line by line
    pclose(fp);

    if (strstr(buf, "0") || (strlen(buf) == 0))
    {
        return 0;
    }

    *buf = 0;
    fp = popen("fw_printenv -n bootlimit", "r");
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    if (strlen(buf)) {
        bootlimit = atoi(buf);
    }

    *buf = 0;
    fp = popen("fw_printenv -n bootcount", "r");
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    if (strlen(buf)) {
        bootcount = atoi(buf);
    }

    if (bootcount > bootlimit) {
        // FOTA boot failed, rollback
        if (FILE_SYSTEM_IS_UBI()) {
            fp = popen("fw_printenv -n boot_partition_alt", "r");
            fgets(buf, sizeof(buf), fp);
            pclose(fp);

            snprintf(cmd, sizeof(cmd), "fw_setenv boot_partition %s", buf);
            ret |= system(cmd);

            fp = popen("fw_printenv -n root_partition_alt", "r");
            fgets(buf, sizeof(buf), fp);
            pclose(fp);

            snprintf(cmd, sizeof(cmd), "fw_setenv root_partition %s", buf);
            ret |= system(cmd);
        }
        var = 1;

    } else {
        if (FILE_SYSTEM_IS_UBI()) {
            fp = popen("fw_printenv -n boot_partition", "r");
            fgets(buf, sizeof(buf), fp);
            pclose(fp);

            snprintf(cmd, sizeof(cmd), "fw_setenv boot_partition_alt %s", buf);
            ret |= system(cmd);

            fp = popen("fw_printenv -n root_partition", "r");
            fgets(buf, sizeof(buf), fp);
            pclose(fp);

            snprintf(cmd, sizeof(cmd), "fw_setenv root_partition_alt %s", buf);
            ret |= system(cmd);

            if (buf[strlen(buf)-1] < '0') {
                buf[strlen(buf)-1] = 0;
            }
            if (0 == (set_ubivol_cmd(buf, cmd, sizeof(cmd)))) {
                ret |= system(cmd);
            }
        }
        var = 2;
    }
    fw_setenv_run_script("bootlimit 0\n"          \
                    "bootcount 0\n"           \
                    "upgrade_available 0\n"   \
                    "boot_partition_alt\n"    \
                    "root_partition_alt\n");

    return var;
}

int set_ubivol_cmd(const char* vol_name, char *o_cmd, int len)
{
    FILE *fp;
    char buf[64], cmd[128];
    int vol_cnt = 0, i;

    if (!FILE_SYSTEM_IS_UBI()) {
        return 0;
    }
    if ((fp = popen("ls -l /dev/ubi0_* | wc -l", "r")) == NULL) {
        return -1;
    }

    fgets(buf, sizeof(buf), fp);
    pclose(fp);

    if (strlen(buf)) {
        vol_cnt = atoi(buf);
    }
    vol_cnt = vol_cnt > 1 ? vol_cnt : 1;
    o_cmd[0] = 0;

    for (i = vol_cnt - 1; i >= 0; i--) {
        snprintf(cmd, sizeof(cmd), "ubinfo /dev/ubi0_%d | grep %s | wc -l", i, vol_name);
        if ((fp = popen(cmd, "r")) == NULL) {
            return -1;
        }
        fgets(buf, sizeof(buf), fp);
        pclose(fp);

        if (strstr(buf, "1")) {
            snprintf(o_cmd, len, "fw_setenv nand_root_alt /dev/ubiblock0_%d", i);
            break;
        }
    }

    if (i < 0) {
        return -1;
    }

    // LOGD(TAG, "cmd:%s", o_cmd);
    return 0;
}
