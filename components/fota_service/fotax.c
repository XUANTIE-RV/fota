/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <cJSON.h>
#include <yoc/fota.h>
#include <ulog/ulog.h>
#include <aos/kv.h>
#include <yoc/sysinfo.h>
#include <aos/version.h>
#include "fotax.h"

#define TAG "fotax"

struct timespec diff_timespec(struct timespec start, struct timespec end)
{
     struct timespec result;
 
     if (end.tv_nsec < start.tv_nsec)
     { 
        result.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;        
        result.tv_sec = end.tv_sec - 1 - start.tv_sec;
     }
     else
     {
        result.tv_nsec = end.tv_nsec - start.tv_nsec;        
        result.tv_sec = end.tv_sec - start.tv_sec;
     }
 
    return result;
}

static int fota_event_cb(void *arg, fota_event_e event)
{
    static int data_offset;
    static struct timespec tv;
    fota_t *fota = (fota_t *)arg;
    fotax_t *fotax = (fotax_t *)fota->private;
    if (!fotax->fotax_event_cb) {
        LOGE(TAG, "fotax event callback is NULL.");
        return -1;
    }

    switch (event) {
        case FOTA_EVENT_START:
            LOGD(TAG, "FOTA START :%x", fota->status);
            break;
        case FOTA_EVENT_VERSION:
        {
            LOGD(TAG, "FOTA VERSION CHECK :%x", fota->status);
            data_offset = 0;
            memset(&tv, 0, sizeof(struct timespec));
            cJSON *root = cJSON_CreateObject();
            if (fota->error_code != FOTA_ERROR_NULL) {
                cJSON_AddNumberToObject(root, "code", 1);
                cJSON_AddStringToObject(root, "msg", "version check failed!");
            } else {
                cJSON_AddNumberToObject(root, "code", 0);
                cJSON_AddStringToObject(root, "curversion", fota->info.cur_version);
                cJSON_AddStringToObject(root, "newversion", fota->info.new_version);
                cJSON_AddNumberToObject(root, "timestamp", fota->info.timestamp);
                cJSON_AddStringToObject(root, "changelog", fota->info.changelog);
                cJSON_AddStringToObject(root, "local_changelog", fota->info.local_changelog);
            }
            char *out = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            if (out != NULL) {
                fotax->fotax_event_cb(fotax, FOTAX_EVENT_VERSION, out);
                cJSON_free(out);
            } else {
                fotax->fotax_event_cb(fotax, FOTAX_EVENT_VERSION, "{\"code\": 1, \"msg\": \"(version)JSON print error!\"}");
            }
            break;
        }
        case FOTA_EVENT_PROGRESS:
        {
            LOGD(TAG, "FOTA PROGRESS :%x, %d, %d", fota->status, fota->offset, fota->total_size);
            int64_t cur_size = fota->offset;
            int64_t total_size = fota->total_size;
            int speed = 0; //kbps
            int percent = 0;
            if (total_size > 0) {
                percent = (int)(cur_size * 100 / total_size);
            }
            cJSON *root = cJSON_CreateObject();
            if (fota->error_code != FOTA_ERROR_NULL) {
                cJSON_AddNumberToObject(root, "code", 1);
                if (fota->error_code == FOTA_ERROR_VERIFY) {
                    cJSON_AddStringToObject(root, "msg", "fota image verify failed!");
                } else {
                    cJSON_AddStringToObject(root, "msg", "download failed!");
                }
                cJSON_AddNumberToObject(root, "total_size", total_size);
                cJSON_AddNumberToObject(root, "cur_size", cur_size);
                cJSON_AddNumberToObject(root, "percent", percent);
            } else {
                // current_size, total_size, percent, speed
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                if (tv.tv_sec > 0 && tv.tv_nsec > 0 && data_offset > 0) {
                    struct timespec diff = diff_timespec(tv, now);
                    int millisecond = diff.tv_sec * 1000 + diff.tv_nsec / 1000000;
                    LOGD(TAG, "interval time: %d ms", millisecond);
                    speed = ((fota->offset - data_offset) / 1024) * 1000 / millisecond;
                }
                cJSON_AddNumberToObject(root, "code", 0);
                cJSON_AddNumberToObject(root, "total_size", total_size);
                cJSON_AddNumberToObject(root, "cur_size", cur_size);
                cJSON_AddNumberToObject(root, "percent", percent);
                cJSON_AddNumberToObject(root, "speed", speed);
                tv.tv_sec = now.tv_sec;
                tv.tv_nsec = now.tv_nsec;
                data_offset = fota->offset;
            }
            char *out = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            if (out != NULL) {
                fotax->fotax_event_cb(fotax, FOTAX_EVENT_DOWNLOAD, out);
                cJSON_free(out);
            } else {
                fotax->fotax_event_cb(fotax, FOTAX_EVENT_DOWNLOAD, "{\"code\": 1, \"msg\": \"(download)JSON print error!\"}");
            }
            break;            
        }
            
        case FOTA_EVENT_FAIL:
            LOGD(TAG, "FOTA FAIL :%x, %d", fota->status, fota->error_code);
            break;
        case FOTA_EVENT_VERIFY:
            LOGD(TAG, "FOTA VERIFY :%x", fota->status);
            break;
        case FOTA_EVENT_FINISH:
            LOGD(TAG, "FOTA FINISH :%x", fota->status);
            fotax->fotax_event_cb(fotax, FOTAX_EVENT_END, "{\"code\": 0, \"msg\": \"fota finish\"}");
            break;
        case FOTA_EVENT_RESTART:
            LOGD(TAG, "FOTA RESTART :%x", fota->status);
            fotax->fotax_event_cb(fotax, FOTAX_EVENT_RESTART, "{\"code\": 0, \"msg\": \"fota restart event\"}");
            break;
        case FOTA_EVENT_QUIT:
            LOGD(TAG, "FOTA QUIT :%x", fota->status);
            break;
        default:
            break;
    }
    return 0;
}

int fotax_start(fotax_t *fotax)
{
    int ret;
    char to_buf[32];
    char cloud_buf[32];
    char *cloud = "cop";
    char *to = "flash://misc";
    int read_timeoutms;         /*!< read timeout, millisecond */
    int write_timeoutms;        /*!< write timeout, millisecond */
    int retry_count;            /*!< when download abort, it will retry to download again in retry_count times */
    int sleep_time;             /*!< the sleep time for auto-check task */
    int auto_check_en;          /*!< whether check version automatic */
    fota_config_t config;

    if (fotax == NULL) {
        LOGE(TAG, "fotax start args e");
        return -EINVAL;
    }
    if (fotax->state != 0) {
        LOGE(TAG, "fotax already started.");
        return -1;
    }
    fotax->fota_handle = NULL;
    LOGD(TAG, "%s, %d", __func__, __LINE__);
    if (fotax->register_ops && fotax->register_ops->fota_register_cloud) {
        ret = fotax->register_ops->fota_register_cloud();
    } else {
        ret = fota_register_cop();
        if (ret != 0) {
            return -1;
        }
        ret = netio_register_http();        
    }
    if (ret != 0) {
        return -1;
    }
    if (fotax->register_ops && fotax->register_ops->netio_register_from) {
        ret = fotax->register_ops->netio_register_from(fotax->register_ops->cert);
    } else {
        ret = netio_register_httpc(NULL);
    }
    if (ret != 0) {
        return -1;
    }
    if (fotax->register_ops && fotax->register_ops->netio_register_to) {
        ret = fotax->register_ops->netio_register_to();
    } else {
        ret = netio_register_flash();
    }
    if (ret != 0) {
        return -1;
    }
    memset(cloud_buf, 0 ,sizeof(cloud_buf));
    ret = aos_kv_getstring(KV_FOTA_CLOUD_PLATFORM, cloud_buf, sizeof(cloud_buf));
    if (ret >= 0) {
        cloud = cloud_buf;
    }
    LOGD(TAG, "cloud: %s", cloud);
    memset(to_buf, 0, sizeof(to_buf));
    ret = aos_kv_getstring(KV_FOTA_TO_URL, to_buf, sizeof(to_buf));
    if (ret >= 0) {
        to = to_buf;
    }
    LOGD(TAG, "to: %s", to);
    fotax->fota_handle = fota_open(cloud, to, fota_event_cb);
    if (fotax->fota_handle == NULL) {
        return -1;
    }
    ((fota_t *)(fotax->fota_handle))->private = fotax;
    if (aos_kv_getint(KV_FOTA_READ_TIMEOUTMS, &read_timeoutms) < 0) {
        read_timeoutms = 3000;
    }
    if (aos_kv_getint(KV_FOTA_WRITE_TIMEOUTMS, &write_timeoutms) < 0) {
        write_timeoutms = 3000;
    }
    if (aos_kv_getint(KV_FOTA_RETRY_COUNT, &retry_count) < 0) {
        retry_count = 0;
    }
    if (aos_kv_getint(KV_FOTA_AUTO_CHECK, &auto_check_en) < 0) {
        auto_check_en = 0;
    }
    if (aos_kv_getint(KV_FOTA_SLEEP_TIMEMS, &sleep_time) < 0) {
        sleep_time = 30000;
    }
    config.read_timeoutms = read_timeoutms;
    config.write_timeoutms = write_timeoutms;
    config.retry_count = retry_count;
    config.auto_check_en = auto_check_en;
    config.sleep_time = sleep_time;
    LOGD(TAG, "read_timeoutms: %d", read_timeoutms);
    LOGD(TAG, "write_timeoutms: %d", write_timeoutms);
    LOGD(TAG, "retry_count: %d", retry_count);
    LOGD(TAG, "auto_check_en: %d", auto_check_en);
    LOGD(TAG, "sleep_time: %d", sleep_time);
    fota_config(fotax->fota_handle, &config);
    ret = fota_start(fotax->fota_handle);
    fotax->state = FOTAX_INIT;
    return ret;
}

int fotax_stop(fotax_t *fotax)
{
    if (fotax == NULL || fotax->fota_handle == NULL) {
        LOGE(TAG, "fotax stop args e");
        return -EINVAL;
    }
    LOGD(TAG, "%s, %d", __func__, __LINE__);
    fota_stop(fotax->fota_handle);
    fota_close(fotax->fota_handle);
    fotax->state = 0;
    fotax->fota_handle = NULL;
    LOGD(TAG, "fotax stop finish. %s, %d", __func__, __LINE__);
    return 0;
}

int fotax_get_state(fotax_t *fotax)
{
    int state;
    if (fotax == NULL || fotax->fota_handle == NULL) {
        LOGE(TAG, "fotax get state args e");
        return -EINVAL;
    }
    state = fota_get_status(fotax->fota_handle);
    LOGD(TAG, "%s, %d, state:%d", __func__, __LINE__, state);
    return state;
}

int fotax_version_check(fotax_t *fotax)
{
    char *device_id, *model, *app_version, *changelog;

    if (fotax == NULL || fotax->fota_handle == NULL) {
        LOGE(TAG, "fotax version check args e");
        return -EINVAL;
    }
    device_id = aos_get_device_id();
    app_version = aos_get_app_version();
    model = (char *)aos_get_product_model();
    changelog = aos_get_changelog();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", 0);
    cJSON_AddStringToObject(root, "curversion", app_version ? app_version : "null");
    cJSON_AddStringToObject(root, "deviceid", device_id ? device_id : "null");
    cJSON_AddStringToObject(root, "model", model ? model : "null");
    cJSON_AddStringToObject(root, "local_changelog", changelog ? changelog : "null");
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (out != NULL) {
        if (fotax->fotax_event_cb) {
            fotax->fotax_event_cb(fotax, FOTAX_EVENT_VERSION, out);
        }
        cJSON_free(out);
    } else {
        fotax->fotax_event_cb(fotax, FOTAX_EVENT_VERSION, "{\"code\": 1, \"msg\": \"(version)JSON print error!\"}");
    }
    LOGD(TAG, "%s, %d", __func__, __LINE__);
    fota_do_check(fotax->fota_handle);
    return 0;
}

int fotax_download(fotax_t *fotax)
{
    if (fotax == NULL || fotax->fota_handle == NULL) {
        LOGE(TAG, "fotax download args e");
        return -EINVAL;
    }
    LOGD(TAG, "%s, %d", __func__, __LINE__);
    fotax->state = FOTAX_DOWNLOAD;
    return fota_download(fotax->fota_handle);
}

int fotax_restart(fotax_t *fotax, int delay_ms)
{
    if (fotax == NULL || fotax->fota_handle == NULL) {
        LOGE(TAG, "fotax restart e");
        return -EINVAL;
    }
    LOGD(TAG, "%s, %d", __func__, __LINE__);
    return fota_restart(fotax->fota_handle, delay_ms);
}

int64_t fotax_get_size(fotax_t *fotax, const char *name)
{
    if (fotax == NULL || fotax->fota_handle == NULL) {
        LOGE(TAG, "fotax get size e");
        return -EINVAL;
    }
    LOGD(TAG, "%s, %d", __func__, __LINE__);
    return fota_get_size(fotax->fota_handle, name);
}