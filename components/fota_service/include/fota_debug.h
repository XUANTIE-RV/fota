/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#ifndef _FOTA_DEBUG_H_
#define _FOTA_DEBUG_H_

#include <stdio.h>
#include <errno.h>
#include <syslog.h>

/*
 * priority, defined in syslog.h
 *
 * #define LOG_EMERG       0       // system is unusable
 * #define LOG_ALERT       1       // action must be taken immediately
 * #define LOG_CRIT        2       // critical conditions
 * #define LOG_ERR         3       // error conditions
 * #define LOG_WARNING     4       // warning conditions
 * #define LOG_NOTICE      5       // normal but significant condition
 * #define LOG_INFO        6       // informational
 * #define LOG_DEBUG       7       // debug-level messages
 */

void fota_log(int priority, const char *format, ...);

#define fota_check(X, errno)                                                                     \
    do {                                                                                          \
        if (!(X)) {                                                                               \
            printf("func: %s, errno: %d\n", __func__, errno);                                     \
            while(1);                                                                             \
        }                                                                                         \
    } while (0)

#define fota_check_param(X) fota_check(X, EINVAL)

#endif /* _FOTA_DEBUG_H_ */
