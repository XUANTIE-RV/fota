/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#include <stdint.h>
#include <ulog/ulog.h>
#include <aos/kv.h>
#include "kv_linux.h"

#define TAG "KV"

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


void __kv_foreach(void (*func)(char *key, char *val, uint16_t val_size, void *arg), void *arg)
{
    if (func == NULL) {
        LOGE(TAG, "%s: param error", __FUNCTION__);
        return;
    }

    aos_kv_mutex_lock(&g_kv_lock, -1);
    kv_iter(&g_kv, func, arg);
    aos_kv_mutex_unlock(&g_kv_lock);
}

int aos_kv_init(const char *pathname)
{
    return __kv_init(pathname);
}

__attribute__((weak)) int aos_kv_set(const char *key, void *value, int len, int sync)
{
    return __kv_setdata((char *)key, value, len);
}

__attribute__((weak)) int aos_kv_setstring(const char *key, const char *v)
{
    return __kv_setdata((char *)key, (void *)v, strlen(v));
}

__attribute__((weak)) int aos_kv_setfloat(const char *key, float v)
{
    return __kv_setdata((char *)key, (void *)&v, sizeof(v));
}

__attribute__((weak)) int aos_kv_setint(const char *key, int v)
{
    return __kv_setdata((char *)key, (void *)&v, sizeof(v));
}

__attribute__((weak)) int aos_kv_get(const char *key, void *buffer, int *buffer_len)
{
    if (buffer_len) {
        int ret = __kv_getdata((char *)key, buffer, *buffer_len);

        if (ret > 0) {
            *buffer_len = ret;
        }

        return (ret <= 0) ? -1 : 0;
    }

    return -1;
}

__attribute__((weak)) int aos_kv_getstring(const char *key, char *value, int len)
{
    int ret;
    ret = __kv_getdata((char *)key, (char *)value, len - 1);

    if (ret > 0) {
        value[ret < len ? ret : len - 1] = '\0';
    }

    return ret;
}

__attribute__((weak)) int aos_kv_getfloat(const char *key, float *value)
{
    int ret = __kv_getdata((char *)key, (char *)value, sizeof(float));
    return (ret == sizeof(float)) ? 0 : -1;
}

__attribute__((weak)) int aos_kv_getint(const char *key, int *value)
{
    int ret = __kv_getdata((char *)key, (char *)value, sizeof(int));
    return (ret == sizeof(int)) ? 0 : -1;
}

__attribute__((weak)) int aos_kv_del(const char *key)
{
    return __kv_del((char *)key);
}

__attribute__((weak)) int aos_kv_reset(void)
{
    return __kv_reset();
}

__attribute__((weak)) void aos_kv_foreach(void (*func)(char *key, char *val, uint16_t val_size, void *arg), void *arg)
{
    __kv_foreach(func, arg);
}
