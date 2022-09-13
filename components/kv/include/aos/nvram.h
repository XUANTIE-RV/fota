/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */

#ifndef YOC_NVRAM_H
#define YOC_NVRAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This function will init nvram.
 *
 * @param[in]   pathname   the data pair of the key, less than 64 bytes
 * @return  0 on success, negative error on failure.
 */
int nvram_init(const char *pathname);

/**
 * This function will reset nvram, clear all data.
 *
 * @return  0 on success, negative error on failure.
 */
int nvram_reset();

/**
 * This function will get data from the factory setting area.
 *
 * @param[in]   key   the data pair of the key, less than 64 bytes
 * @param[in]   len   the length of the buffer
 * @param[out]  value   the buffer that will store the data
 * @return  the length of the data value, error code otherwise
 */
int nvram_get_val(const char *key, char *value, int len);

/**
 * This function will set data to the factory setting area.
 *
 * @param[in]   key   the data pair of the key, less than 64 bytes
 * @param[in]   value the data pair of the value, delete the pair if value == NULL
 * @return  0 on success, negative error on failure.
 */
int nvram_set_val(const char *key, char *value);

#ifdef __cplusplus
}
#endif

#endif

