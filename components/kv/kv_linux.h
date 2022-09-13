/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#ifndef AOS_KVSET_H
#define AOS_KVSET_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define KV_MAX_KEYPATH_LEN 1024
#define KV_MAX_KEY_LEN     256
#define KV_MAX_VALUE_LEN   4096

typedef struct kvset kv_t;

struct kvset {
    int  handle;
    char path[KV_MAX_KEYPATH_LEN - KV_MAX_KEY_LEN];
};

/**
 * @brief  init the kv fs
 * @param  [in] kv          : object must be alloced
 * @param  [in] path        : kv set base path
 * @return 0/-1
 */
int kv_init(kv_t *kv, const char *path);

/**
 * @brief  reset the kv fs
 * @param  [in] kv
 * @return 0/-1
 */
int kv_reset(kv_t *kv);

/**
 * @brief  set key-value pair
 * @param  [in] kv
 * @param  [in] key
 * @param  [in] value
 * @param  [in] size  : size of the value
 * @return size on success
 */
int kv_set(kv_t *kv, const char *key, void *value, int size);

/**
 * @brief  get value by the key-string
 * @param  [in] kv
 * @param  [in] key
 * @param  [in] value
 * @param  [in] size  : size of the value
 * @return > 0 on success
 */
int kv_get(kv_t *kv, const char *key, void *value, int size);

/**
 * @brief  delete the key from kv fs
 * @param  [in] kv
 * @param  [in] key
 * @return 0 on success
 */
int kv_rm(kv_t *kv, const char *key);

/**
 * @brief  iterate all valid kv pair
 * @param  [in] kv
 * @param  [in] func   : callback
 * @param  [in] arg    : opaque of the fn callback
 * @return 0 on success
 */
void kv_iter(kv_t *kv, void (*func)(char *key, char *val, uint16_t val_size, void *arg), void *arg);
/********************************
 * MUTEX
 ********************************/
#include <pthread.h>
typedef struct __aos_kv_mutex_t {
    void *      hdl;
    const char *key_file;
} aos_kv_mutex_t;

/**
 * Alloc a mutex.
 *
 * @param[in]  mutex  pointer of mutex object, mutex object must be alloced,
 *                    hdl pointer in aos_kv_mutex_t will refer a kernel obj internally.
 *
 * @return  0: success.
 */
int aos_kv_mutex_new(aos_kv_mutex_t *mutex);

/**
 * Free a mutex.
 *
 * @param[in]  mutex  mutex object, mem refered by hdl pointer in aos_kv_mutex_t will
 *                    be freed internally.
 */
void aos_kv_mutex_free(aos_kv_mutex_t *mutex);

/**
 * Lock a mutex.
 *
 * @param[in]  mutex    mutex object, it contains kernel obj pointer which aos_kv_mutex_new alloced.
 * @param[in]  timeout  waiting until timeout in milliseconds.
 *
 * @return  0: success.
 */
int aos_kv_mutex_lock(aos_kv_mutex_t *mutex, unsigned int timeout);

/**
 * Unlock a mutex.
 *
 * @param[in]  mutex  mutex object, it contains kernel obj pointer which oc_mutex_new alloced.
 *
 * @return  0: success.
 */
int aos_kv_mutex_unlock(aos_kv_mutex_t *mutex);

#ifdef __cplusplus
}
#endif

#endif
