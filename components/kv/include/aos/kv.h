/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#ifndef AOS_KV_H
#define AOS_KV_H

// #include <aos/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <string.h>

#define KV_PATH_DEFULT "/data/kv/kv"
#ifndef CONFIG_NV_PATH
#define NV_PATH_DEFUALT "/mnt/PARAM/nv"
#else
#define NV_PATH_DEFUALT CONFIG_NV_PATH
#endif

/**
 * This function will init kv system
 * @return  0 on success, negative error on failure.
 */
int aos_kv_init(const char *pathname);

/**
 * This function will reset all kv, restore factory settting
 * @return  0 on success, negative error on failure.
 */
int aos_kv_reset(void);

/**
 * Add a new KV pair.
 *
 * @param[in]  key    the key of the KV pair.
 * @param[in]  value  the value of the KV pair.
 * @param[in]  len    the length of the value.
 * @param[in]  sync   save the KV pair to flash right now (should always be 1).
 *
 * @return  0 on success, negative error on failure.
 */
int aos_kv_set(const char *key, void *value, int len, int sync);

int aos_kv_setfloat(const char *key, float v);

int aos_kv_setint(const char *key, int v);

/**
 * Get the KV pair's value stored in buffer by its key.
 *
 * @note: the buffer_len should be larger than the real length of the value,
 *        otherwise buffer would be NULL.
 *
 * @param[in]      key         the key of the KV pair to get.
 * @param[out]     buffer      the memory to store the value.
 * @param[in-out]  buffer_len  in: the length of the input buffer.
 *                             out: the real length of the value.
 *
 * @return  0 on success, negative error on failure.
 */
int aos_kv_get(const char *key, void *buffer, int *buffer_len);

int aos_kv_getfloat(const char *key, float *value);

int aos_kv_getint(const char *key, int *value);

/**
 *
 * @param[in]  key    the key of the KV pair.
 * @param[in]  value  the value of the KV pair.
 * @param[in]  len    the length of the value.
 * @param[in]  sync   save the KV pair to flash right now (should always be 1).
 *
 * @setstring return  0 on success, negative error on failure.
 * @getstring return  >=0 on success(read len), negative error on failure.
 */
int aos_kv_setstring(const char *key, const char *v);
int aos_kv_getstring(const char *key, char *value, int len);

/**
 * Delete the KV pair by its key.
 *
 * @param[in]  key  the key of the KV pair to delete.
 *
 * @return  0 on success, negative error on failure.
 */
int aos_kv_del(const char *key);

void aos_kv_foreach(void (*func)(char *key, char *val, uint16_t val_size, void *arg), void *arg);

#ifdef __cplusplus
}
#endif

#endif /* AOS_KV_H */
