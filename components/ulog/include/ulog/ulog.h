/**
 * @file ulog/ulog.h
 * @copyright Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#ifndef ULOG_H
#define ULOG_H

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ULOG_TAG __FILE__, __LINE__

#if 0
/*already define in syslog.h*/
#define LOG_EMERG   0 /* system is unusable */
#define LOG_ALERT   1 /* action must be taken immediately */
#define LOG_CRIT    2 /* critical conditions */
#define LOG_ERR     3 /* error conditions */
#define LOG_WARNING 4 /* warning conditions */
#define LOG_NOTICE  5 /* normal, but significant, condition */
#define LOG_INFO    6 /* informational message */
#define LOG_DEBUG   7 /* debug-level message */
#define LOG_NONE    8 /* used in stop filter, all log will pop out */
#endif

typedef enum {
    AOS_LL_NONE  = LOG_EMERG,   /* disable log */
    AOS_LL_FATAL = LOG_CRIT,    /* fatal log will output */
    AOS_LL_ERROR = LOG_ERR,     /* fatal + error log will output */
    AOS_LL_WARN  = LOG_WARNING, /* fatal + warn + error log will output(default level) */
    AOS_LL_INFO  = LOG_INFO,    /* info + warn + error log will output */
    AOS_LL_DEBUG = LOG_DEBUG,   /* debug + info + warn + error + fatal log will output */
} aos_log_level_t;

int ulog(const unsigned char lvl, const char *mod, const char *f,
         const unsigned long l, const char *fmt, ...);

/**
 * Log at the alert level, brief using of ulog.
 *
 * @param[in]  ...  same as printf() usage.
 *
 * @return  0 on success, negative error on failure.
 */
#define LOG(...) ulog(LOG_ALERT, "AOS", ULOG_TAG, __VA_ARGS__)

#ifdef CONFIG_LOGMACRO_SILENT
#define LOGF(mod, ...)
#define LOGE(mod, ...)
#define LOGW(mod, ...)
#define LOGI(mod, ...)
#define LOGD(mod, ...)

#else /* !CONFIG_LOGMACRO_SILENT */
/**
 * Log at fatal level, brief using of ulog.
 *
 * @param[in]  mod  string description of module.
 * @param[in]  ...  same as printf() usage.
 *
 * @return  0 on success, negative error on failure.
 */
#define LOGF(mod, ...) ulog(LOG_CRIT, mod, ULOG_TAG, __VA_ARGS__)

/**
 * Log at error level, brief using of ulog.
 *
 * @param[in]  mod  string description of module.
 * @param[in]  ...  same as printf() usage.
 *
 * @return  0 on success, negative error on failure.
 */
#define LOGE(mod, ...) ulog(LOG_ERR, mod, ULOG_TAG, __VA_ARGS__)

/**
 * Log at warning level, brief using of ulog.
 *
 * @param[in]  mod  string description of module.
 * @param[in]  ...  same as printf() usage.
 *
 * @return  0 on success, negative error on failure.
 */
#define LOGW(mod, ...) ulog(LOG_WARNING, mod, ULOG_TAG, __VA_ARGS__)

/**
 * Log at info level, brief using of ulog.
 *
 * @param[in]  mod  string description of module.
 * @param[in]  ...  same as printf() usage.
 *
 * @return  0 on success, negative error on failure.
 */
#define LOGI(mod, ...) ulog(LOG_INFO, mod, ULOG_TAG, __VA_ARGS__)

/**
 * Log at debug level, brief using of ulog.
 *
 * @note: This Log API take effect only the switcher 'DEBUG' is switch on.
 * @param[in]  mod  string description of module.
 * @param[in]  ...  same as printf() usage.
 *
 * @return  0 on success, negative error on failure.
 */
#define LOGD(mod, ...) ulog(LOG_DEBUG, mod, ULOG_TAG,  __VA_ARGS__)

#endif /* CONFIG_LOGMACRO_SILENT */

int aos_set_log_level(aos_log_level_t log_level);

#ifdef __cplusplus
}
#endif

#endif /* ULOG_H */

