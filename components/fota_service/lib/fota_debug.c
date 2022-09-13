/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdarg.h>

int daemonize = 0;

void fota_log(int priority, const char *format, ...)
{
    va_list arg;

    if (daemonize == 1) {
        openlog("fota", LOG_CONS | LOG_PID, LOG_LOCAL0);
        va_list args;
        va_start(args, format);
        vsyslog(priority, format, args);
        va_end(args);
        closelog();
    } else {
        va_start(arg, format);
        printf("[FotaService] ");
        vfprintf(stdout, format, arg);
        va_end(arg);
    }
}
