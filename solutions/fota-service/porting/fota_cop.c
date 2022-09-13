/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <yoc/fota.h>
#include <yoc/netio.h>
#include <aos/kv.h>
#include <aos/kernel.h>
#include <ulog/ulog.h>
#include <yoc/sysinfo.h>
#include <aos/version.h>
#include <sys/statvfs.h>
#include "imagef.h"

#define COP_IMG_URL "cop_img_url"
#define COP_VERSION "cop_version"

#define TAG "fotacop"

#include <http_client.h>
#include <cJSON.h>
static int cop_get_ota_url(char *ota_url, int len)
{
    int ret = -1;

    ret = aos_kv_getstring("otaurl", ota_url, len);
    if (ret < 0) {
        strcpy(ota_url, "http://occ.t-head.cn/api/image/ota/pull");
    }

    return ret;
}

static int _http_event_handler(http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            // LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return 0;
}

static bool process_again(int status_code)
{
    switch (status_code) {
        case HttpStatus_MovedPermanently:
        case HttpStatus_Found:
        case HttpStatus_TemporaryRedirect:
        case HttpStatus_Unauthorized:
            return true;
        default:
            return false;
    }
    return false;
}

static http_errors_t _http_handle_response_code(http_client_handle_t http_client, int status_code, char *buffer, int buf_size, int data_size)
{
    http_errors_t err;
    if (status_code == HttpStatus_MovedPermanently || status_code == HttpStatus_Found || status_code == HttpStatus_TemporaryRedirect) {
        err = http_client_set_redirection(http_client);
        if (err != HTTP_CLI_OK) {
            LOGE(TAG, "URL redirection Failed");
            return err;
        }
    } else if (status_code == HttpStatus_Unauthorized) {
        return HTTP_CLI_FAIL;
    } else if(status_code == HttpStatus_NotFound || status_code == HttpStatus_Forbidden) {
        LOGE(TAG, "File not found(%d)", status_code);
        return HTTP_CLI_FAIL;
    } else if (status_code == HttpStatus_InternalError) {
        LOGE(TAG, "Server error occurred(%d)", status_code);
        return HTTP_CLI_FAIL;
    }

    // process_again() returns true only in case of redirection.
    if (data_size > 0 && process_again(status_code)) {
        /*
        *  In case of redirection, http_client_read() is called
        *  to clear the response buffer of http_client.
        */
        int data_read;
        while (data_size > buf_size) {
            data_read = http_client_read(http_client, buffer, buf_size);
            if (data_read <= 0) {
                return HTTP_CLI_OK;
            }
            data_size -= buf_size;
        }
        data_read = http_client_read(http_client, buffer, data_size);
        if (data_read <= 0) {
            return HTTP_CLI_OK;
        }
    }
    return HTTP_CLI_OK;
}

static http_errors_t _http_connect(http_client_handle_t http_client, const char *payload, char *buffer, int buf_size)
{
#define MAX_REDIRECTION_COUNT 10
    http_errors_t err = HTTP_CLI_FAIL;
    int status_code = 0, header_ret;
    int redirect_counter = 0;

    do {
        if (redirect_counter++ > MAX_REDIRECTION_COUNT) {
            LOGE(TAG, "redirect_counter is max");
            return HTTP_CLI_FAIL;
        }
        if (process_again(status_code)) {
            LOGD(TAG, "process again,status code:%d", status_code);
        }
        err = http_client_open(http_client, strlen(payload));
        if (err != HTTP_CLI_OK) {
            LOGE(TAG, "Failed to open HTTP connection");
            return err;
        }
        int wlen = http_client_write(http_client, payload, strlen(payload));
        if (wlen < 0) {
            LOGE(TAG, "Write payload failed");
            return HTTP_CLI_FAIL;
        }
        LOGD(TAG, "write payload ok...");
        header_ret = http_client_fetch_headers(http_client);
        if (header_ret < 0) {
            LOGE(TAG, "header_ret:%d", header_ret);
            return header_ret;
        }
        LOGD(TAG, "header_ret:%d", header_ret);
        status_code = http_client_get_status_code(http_client);
        LOGD(TAG, "status code:%d", status_code);
        err = _http_handle_response_code(http_client, status_code, buffer, buf_size, header_ret);
        if (err != HTTP_CLI_OK) {
            LOGE(TAG, "e handle resp code:%d", err);
            return err;
        }
    } while (process_again(status_code));
    return err;
}

static void _http_cleanup(http_client_handle_t client)
{
    if (client) {
        http_client_cleanup(client);
    }
}

static int sw_partition(download_img_info_t *dl_img_info)
{
    int ret;
    char cmd[128];

    for (int i = 0; i < dl_img_info->image_count; i++) {
        if (strcmp(dl_img_info->img_info[i].img_name, IMG_NAME_DIFF) == 0) {
            ret = check_rootfs_partition();
            if (ret != 1 && ret != 2) {
                LOGE(TAG, "Check rootfs partition failed");
                remove("/"IMG_NAME_DIFF);
                return -1;
            }
            if (system("ota-burndiff /"IMG_NAME_DIFF) != 0)
            {
                LOGE(TAG, "Exec ota-diff failed");
                remove("/"IMG_NAME_DIFF);
                return -1;
            }
        }
    }
    for (int i = 0; i < dl_img_info->image_count; i++) {
        if (strcmp(dl_img_info->img_info[i].img_name, IMG_NAME_UBOOT) == 0) {
            LOGD(TAG, "got uboot the dev name: %s, img_path: %s", dl_img_info->img_info[i].dev_name, dl_img_info->img_info[i].img_path);
            // snprintf(cmd, sizeof(cmd), "dd if=%s of=%s >/dev/null", dl_img_info->img_info[i].img_path, dl_img_info->img_info[i].dev_name);
            snprintf(cmd, sizeof(cmd), "ota-burnuboot %s > /dev/null", dl_img_info->img_info[i].img_path);
            LOGD(TAG, "cmd: %s", cmd);
            ret = system(cmd);
        } else if (strcmp(dl_img_info->img_info[i].img_name, IMG_NAME_KERNEL) == 0) {
            ret = check_kernel_partition();
            if (ret == 1)
            {
                // A -> B
                LOGD(TAG, "kernel Switch A -> B");
                ret = system("fw_setenv boot_partition bootB");
                ret |= system("fw_setenv boot_partition_alt bootA");
            }
            else if (ret == 2)
            {
                // B -> A
                LOGD(TAG, "kernel Switch B -> A");
                ret = system("fw_setenv boot_partition bootA");
                ret |= system("fw_setenv boot_partition_alt bootB");
            }
            else
            {
                LOGE(TAG, "Check kernel partition failed");
            }
        } else if (strcmp(dl_img_info->img_info[i].img_name, IMG_NAME_ROOTFS) == 0) {
            ret = check_rootfs_partition();
            if (ret == 1)
            {
                // A -> B
                LOGD(TAG, "rootfs Switch A -> B");
                ret = system("fw_setenv root_partition rootfsB");
                ret |= system("fw_setenv root_partition_alt rootfsA");
                if (0 == (set_ubivol_cmd("rootfsA", cmd, sizeof(cmd)))) {
                    ret |= system(cmd);
                } else {
                    ret = -1;
                }
            }
            else if (ret == 2)
            {
                // B -> A
                LOGD(TAG, "rootfs Switch B -> A");
                ret = system("fw_setenv root_partition rootfsA");
                ret |= system("fw_setenv root_partition_alt rootfsB");
                if (0 == (set_ubivol_cmd("rootfsB", cmd, sizeof(cmd)))) {
                    ret |= system(cmd);
                } else {
                    ret = -1;
                }
            }
            else
            {
                LOGE(TAG, "Check rootfs partition failed");
            }
        } else if (check_partition_exists(dl_img_info->img_info[i].img_name)) {
            ret = check_partition_ab(dl_img_info->img_info[i].img_name);
            if (ret == 1)
            {
                // A -> B
                LOGD(TAG, "%s Switch A -> B", dl_img_info->img_info[i].img_name);
                snprintf(cmd, sizeof(cmd) - 1, "fw_setenv %s_partition B", dl_img_info->img_info[i].img_name);
                ret = system(cmd);
                snprintf(cmd, sizeof(cmd) - 1, "fw_setenv %s_partition_alt A", dl_img_info->img_info[i].img_name);
                ret |= system(cmd);
            }
            else if (ret == 2)
            {
                // B -> A
                LOGD(TAG, "%s Switch B -> A", dl_img_info->img_info[i].img_name);
                snprintf(cmd, sizeof(cmd) - 1, "fw_setenv %s_partition A", dl_img_info->img_info[i].img_name);
                ret = system(cmd);
                snprintf(cmd, sizeof(cmd) - 1, "fw_setenv %s_partition_alt B", dl_img_info->img_info[i].img_name);
                ret |= system(cmd);
            }
            else
            {
                LOGE(TAG, "Check %s partition failed", dl_img_info->img_info[i].img_name);
            }
        } else {
            LOGE(TAG, "the image name is error.[%s]", dl_img_info->img_info[i].img_name);
        }
    }
    if (system("sync")) {
        LOGE(TAG, "sync failed.");
    }
    return 0;
}

static void cop_res_release(fota_info_t *info)
{
    if (info) {
        if (info->fota_url) {
            aos_free(info->fota_url);
            info->fota_url = NULL;
        }
        if (info->local_changelog) {
            aos_free(info->local_changelog);
            info->local_changelog = NULL;
        }
        if (info->changelog) {
            aos_free(info->changelog);
            info->changelog = NULL;
        }
        if (info->cur_version) {
            aos_free(info->cur_version);
            info->cur_version = NULL;
        }
        if (info->new_version) {
            aos_free(info->new_version);
            info->new_version = NULL;
        }
    }
}

static int cop_version_check(fota_info_t *info) {
#define BUF_SIZE 156
#define URL_SIZE 256
    int ret = 0, rc;
    char *payload = NULL;
    char getvalue[64];
    char *device_id, *model, *app_version;
    cJSON *js = NULL;
    char *buffer = NULL;
    char *urlbuf = NULL;
    http_errors_t err;
    http_client_config_t config = {0};
    http_client_handle_t client = NULL;

    if (info == NULL) {
        return -EINVAL;
    }

    buffer = aos_zalloc(BUFFER_SIZE + 1);
    if (buffer == NULL) {
        ret = -ENOMEM;
        goto out;
    }
    if ((payload = aos_malloc(BUF_SIZE)) == NULL) {
        ret = -ENOMEM;
        goto out;
    }

    device_id = aos_get_device_id();
    app_version = aos_get_app_version();
    model = (char *)aos_get_product_model();
    snprintf(payload, BUF_SIZE, "{\"cid\":\"%s\",\"model\":\"%s\",\"version\":\"%s\"}",
            device_id ? device_id : "null", model ? model : "null", app_version ? app_version : "null");

    LOGD(TAG, "request playload: %s", payload);

    memset(getvalue, 0, sizeof(getvalue));
    cop_get_ota_url(getvalue, sizeof(getvalue));
    LOGD(TAG, "ota url:%s", getvalue);

    int timeout_ms;
    if (aos_kv_getint(KV_FOTA_READ_TIMEOUTMS, &timeout_ms) < 0) {
        timeout_ms = 10000;
    }

    config.method = HTTP_METHOD_POST;
    config.url = getvalue;
    config.timeout_ms = timeout_ms;
    config.buffer_size = BUFFER_SIZE;
    config.event_handler = _http_event_handler;
    LOGD(TAG, "http client init start.");
    client = http_client_init(&config);
    if (!client) {
        LOGE(TAG, "Client init e");
        ret = -1;
        goto out;
    }
    LOGD(TAG, "http client init ok.");
    http_client_set_header(client, "Content-Type", "application/json");
    http_client_set_header(client, "Connection", "keep-alive");
    http_client_set_header(client, "Cache-Control", "no-cache");
    err = _http_connect(client, payload, buffer, BUFFER_SIZE);
    if (err != HTTP_CLI_OK) {
        LOGE(TAG, "Client connect e");
        ret = -1;
        goto out;
    }
    int read_len = http_client_read(client, buffer, BUFFER_SIZE);
    if (read_len <= 0) {
        ret = -1;
        goto out;
    }
    buffer[read_len] = 0;
    LOGD(TAG, "resp: %s", buffer);

    js = cJSON_Parse(buffer);
    if (js == NULL) {
        ret = -1;
        LOGW(TAG, "cJSON_Parse failed");
        goto out;
    }

    cJSON *code = cJSON_GetObjectItem(js, "code");
    if (!(code && cJSON_IsNumber(code))) {
        ret = -1;
        LOGW(TAG, "get code failed");
        goto out;
    }
    LOGD(TAG, "code: %d", code->valueint);
    if (code->valueint < 0) {
        ret = -1;
        goto out;
    }

    cJSON *result = cJSON_GetObjectItem(js, "result");
    if (!(result && cJSON_IsObject(result))) {
        LOGW(TAG, "get result failed");
        ret = -1;
        goto out;
    }

    cJSON *version = cJSON_GetObjectItem(result, "version");
    if (!(version && cJSON_IsString(version))) {
        LOGW(TAG, "get version failed");
        ret = -1;
        goto out;
    }
    LOGD(TAG, "version: %s", version->valuestring);
    aos_kv_setstring(COP_VERSION, version->valuestring);

    cJSON *url = cJSON_GetObjectItem(result, "url");
    if (!(url && cJSON_IsString(url))) {
        ret = -1;
        LOGW(TAG, "get url failed");
        goto out;
    }
    LOGD(TAG, "url: %s", url->valuestring);

    urlbuf = aos_malloc(URL_SIZE);
    if (urlbuf == NULL) {
        ret = -1;
        goto out;
    }
    rc = aos_kv_getstring(COP_IMG_URL, urlbuf, URL_SIZE);
    if (rc <= 0) {
        aos_kv_setstring(COP_IMG_URL, url->valuestring);
    } else {
        if (strcmp(url->valuestring, urlbuf) == 0) {
            if (aos_kv_getint(KV_FOTA_OFFSET, &rc) < 0) {
                rc = 0;
            }
            LOGI(TAG, "-------->>>continue fota, offset: %d", rc);
        } else {
            aos_kv_setstring(COP_IMG_URL, url->valuestring);
            if (aos_kv_setint(KV_FOTA_OFFSET, 0) < 0) {
                ret = -1;
                goto out;
            }
            LOGI(TAG, "restart fota");
        }
    }

    cJSON *changelog = cJSON_GetObjectItem(result, "changelog");
    if (!(changelog && cJSON_IsString(changelog))) {
        LOGW(TAG, "get changelog failed");
    } else {
        LOGD(TAG, "changelog: %s", changelog->valuestring);
    }

    cJSON *timestamp = cJSON_GetObjectItem(result, "timestamp");
    if (!(timestamp && cJSON_IsNumber(timestamp))) {
        LOGW(TAG, "get timestamp failed");
        info->timestamp = 0;
    } else {
        LOGD(TAG, "timestamp: %d", timestamp->valueint);
        info->timestamp = timestamp->valueint;
    }

    if (info->fota_url) {
        aos_free(info->fota_url);
        info->fota_url = NULL;
    }
    if (info->changelog) {
        aos_free(info->changelog);
        info->changelog = NULL;
    }
    if (info->new_version) {
        aos_free(info->new_version);
        info->new_version = NULL;
    }

    info->fota_url = strdup(url->valuestring);
    if (changelog && changelog->valuestring) {
        info->changelog = strdup(changelog->valuestring);
    } else {
        info->changelog = strdup("fix bug...");
    }
    aos_kv_setstring("newchangelog", info->changelog);
    info->new_version = strdup(version->valuestring);
    LOGD(TAG, "get url: %s", info->fota_url);
    LOGD(TAG, "get changelog: %s", info->changelog);
    goto success;
out:
    LOGE(TAG, "fota cop version check failed.");
success:
    if (urlbuf) aos_free(urlbuf);
    if (buffer) aos_free(buffer);
    if (payload) aos_free(payload);
    if (js) cJSON_Delete(js);
    _http_cleanup(client);
    return ret;
}

static int cop_init(fota_info_t *info)
{
    char *version, *changelog;
    LOGD(TAG, "%s, %d", __func__, __LINE__);

    if (access(IMGINFOFILE, F_OK) != 0) {
        LOGD(TAG, "there is no %s file.", IMGINFOFILE);
        if (aos_kv_setint(KV_FOTA_OFFSET, 0) < 0) {
            LOGE(TAG, "cop init set fota offset 0 failed.");
            return -1;
        }
    }
    version = aos_get_app_version();
    LOGD(TAG, "version: %s", version ? version : "");
    if (info) info->cur_version = strdup(version ? version : "null");
    changelog = aos_get_changelog();
    LOGD(TAG, "changelog: %s", changelog ? changelog : "");
    if (info) info->local_changelog = strdup(changelog ? changelog : "null");
    return 0;
}

static int finish(fota_info_t *info)
{
    cop_res_release(info);
    return 0;
}

static int fail(fota_info_t *info)
{
    cop_res_release(info);
    return 0;
}

static int restart(void)
{
    FILE *fp;
    int ret;
    download_img_info_t dl_img_info;

    LOGD(TAG, "real to do restart opration......");
    fp = fopen(IMGINFOFILE, "rb+");
    if (fp != NULL) {
        int len = get_file_size(fp, -1);
        if (len < sizeof(download_img_info_t)) {
            LOGE(TAG, "The %s file length is wrong.", IMGINFOFILE);
            ret = -1;
            goto out;
        }
        if (fread(&dl_img_info, 1, sizeof(download_img_info_t), fp) < sizeof(download_img_info_t)) {
            LOGE(TAG, "Read %s file error.", IMGINFOFILE);
            ret = -1;
            goto out;
        }
        fclose(fp);
        fp = NULL;
        if (sw_partition(&dl_img_info) < 0) {
            return -1;
        }
        set_rollback_env_param(5);          // set rollback bootlimit=5
        ret = system("reboot -n");
    } else {
        ret = -1;
        LOGE(TAG, "Cant find %s file.", IMGINFOFILE);
    }
out:
    if (fp) fclose(fp);
    return ret;
}


static int filesystem_get_size(const char *path, uint64_t *used_size, uint64_t *available_size)
{
	struct statvfs vfs;

	if (statvfs(path, &vfs) < 0) {
        return -1;
	}
	if (used_size) {
        *used_size = (vfs.f_blocks - vfs.f_bfree) * vfs.f_bsize;
    }
	if (available_size) {
        *available_size = vfs.f_bavail * vfs.f_bsize;
    }

	return 0;
}

static int64_t get_size(const char *name)
{
    const char *mount_point[] = {"/", "/tmp/fota__boot"};
    const char *partitions_name[] = {"root", "boot"}; 
    const char cmd_boot_mount[] = "#!/bin/bash\nset -e\n"           \
        "CURPART=$(fw_printenv -n boot_partition)\n"                \
        "if [ \"${CURPART}\" == \"bootB\" ];\n"                        \
        "then\n"                                                    \
        "    BOOTDEV=`blkid -t PARTLABEL=bootB -o device`\n"        \
        "else\n"                                                    \
        "    BOOTDEV=`blkid -t PARTLABEL=boot -o device`\n"         \
        "fi\n"                                                      \
        "mkdir -p /tmp/fota__boot\n"                                \
        "if `grep -qs $BOOTDEV /proc/mounts`; then\n"               \
        "	umount $BOOTDEV;\n"                                     \
        "fi\n"                                                      \
        "mount $BOOTDEV /tmp/fota__boot\n";
    const char cmd_boot_umount[] = "umount /tmp/fota__boot && rm -rf /tmp/fota__boot";
    int i;
    uint64_t size;

    if (name == NULL) {
        if (filesystem_get_size(mount_point[0], NULL, &size) < 0) {
            return -1;
        }
        return ((int64_t)size);
    }

    for (i = 0; i < sizeof(mount_point) / sizeof(mount_point[0]); i++) {
        if (!strcmp(name, partitions_name[i])) {
            if (!strcmp(name, "boot")) {
                if (system(cmd_boot_mount)) {
                    return -1;
                }
            }
            if (filesystem_get_size(mount_point[i], NULL, &size) < 0) {
                if (!strcmp(name, "boot")) {
                    if (system(cmd_boot_umount)) {
                        LOGE(TAG, "umount boot failed.");
                    }
                }
                return -1;
            }
            if (!strcmp(name, "boot")) {
                if (system(cmd_boot_umount)) {
                    LOGE(TAG, "umount boot failed.");
                }
            }
            return ((int64_t)size);
        }
    }

    return -1;
}

int set_fota_update_version_ok(void)
{
    int ab;
    int ret;
    char buffer[512];

    LOGD(TAG, "%s, %d", __func__, __LINE__);
    memset(buffer, 0, sizeof(buffer));
    ab = check_rootfs_partition();
    ret = aos_kv_getstring(COP_VERSION, buffer, sizeof(buffer));
    if (ret > 0) {
        if (ab == 1) {
            aos_kv_setstring("versionA", buffer);
        } else {
            aos_kv_setstring("versionB", buffer);
        }
        if (aos_set_app_version(buffer) < 0) {
            LOGE(TAG, "set app version failed.");
            return -1;
        }
        LOGD(TAG, "set app version ok [%s]", buffer);
    }

    memset(buffer, 0, sizeof(buffer));
    ret = aos_kv_getstring("newchangelog", buffer, sizeof(buffer));
    if (ret > 0) {
        if (ab == 1) {
            aos_kv_setstring("changelogA", buffer);
        } else {
            aos_kv_setstring("changelogB", buffer);
        }
        if (aos_set_changelog(buffer) < 0) {
            LOGE(TAG, "set changelog failed.");
            return -1;
        }
        LOGD(TAG, "set changelog ok [%s]", buffer);
    }

    LOGD(TAG, "versionAB&changelogAB set finish.");
    return 0;
}

const fota_cls_t fota_cop2_cls = {
    "cop2",
    cop_init,
    cop_version_check,
    finish,
    fail,
    restart,
    get_size
};

int fota_register_cop2(void)
{
    return fota_register(&fota_cop2_cls);
}