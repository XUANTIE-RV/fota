/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#include <ulog/ulog.h>
#include <aos/kv.h>
#include <aos/nvram.h>
#include "kv_linux.h"

#define TAG "NV"

static kv_t        g_kv;
static aos_kv_mutex_t g_kv_lock;

static int __kv_init(const char *pathname)
{
    g_kv_lock.key_file = pathname;
    int ret = kv_init(&g_kv, pathname);

    /* shmkey ftok need real path, must call after kv_init */
    aos_kv_mutex_new(&g_kv_lock);
    return ret;
}

static int __kv_setdata(char *key, char *buf, int bufsize)
{
    if (g_kv.handle < 0) {
        return -1;
    }

    aos_kv_mutex_lock(&g_kv_lock, -1);
    int ret = kv_set(&g_kv, key, buf, bufsize) >= 0 ? 0 : -1;
    aos_kv_mutex_unlock(&g_kv_lock);

    return ret;
}

static int __kv_getdata(char *key, char *buf, int bufsize)
{
    if (g_kv.handle < 0) {
        return -1;
    }

    if (key == NULL || buf == NULL || bufsize <= 0)
        return -1;
    aos_kv_mutex_lock(&g_kv_lock, -1);
    int ret = kv_get(&g_kv, key, buf, bufsize);
    aos_kv_mutex_unlock(&g_kv_lock);

    return ret;
}

static int __kv_del(char *key)
{
    if (g_kv.handle < 0) {
        return -1;
    }

    aos_kv_mutex_lock(&g_kv_lock, -1);
    int ret = kv_rm(&g_kv, key);
    aos_kv_mutex_unlock(&g_kv_lock);

    return ret;
}

static int __kv_reset(void)
{
    if (g_kv.handle < 0) {
        return -1;
    }

    aos_kv_mutex_lock(&g_kv_lock, -1);
    int ret = kv_reset(&g_kv);
    aos_kv_mutex_unlock(&g_kv_lock);

    return ret;
}

/*************************
 * Set Get API
 *************************/
int nvram_init(const char *pathname)
{
    return __kv_init(pathname);
}
int nvram_get_val(const char *key, char *value, int len)
{
    int ret;
    ret = __kv_getdata((char *)key, (char *)value, len - 1);

    if(ret > 0) {
        value[ret < len ? ret : len - 1] = '\0';
    }

    return ret;
}

int nvram_set_val(const char *key, char *value)
{
    return __kv_setdata((char *)key, (void *)value, strlen(value));
}

int nvram_del(const char *key)
{
    return __kv_del((char *)key);
}

int nvram_reset(void)
{
    return __kv_reset();
}
