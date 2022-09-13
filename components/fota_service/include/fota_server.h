/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#ifndef __FOTA_SERVER_H__
#define __FOTA_SERVER_H__

#include <stddef.h>
#include <pthread.h>
#include <dbus/dbus.h>

#include <fota.h>

#ifdef __cplusplus
extern "C" {
#endif

int fota_dbus_signal_version(fota_server_t *fota, const char *str);
int fota_dbus_signal_download(fota_server_t *fota, const char *str);
int fota_dbus_signal_end(fota_server_t *fota, const char *str);
int fota_dbus_signal_restart(fota_server_t *fota, const char *str);
int fota_dbus_method_start(DBusMessage *msg, fota_server_t *fota);
int fota_dbus_method_stop(DBusMessage *msg, fota_server_t *fota);
int fota_dbus_method_get_state(DBusMessage *msg, fota_server_t *fota);
int fota_dbus_method_version_check(DBusMessage *msg, fota_server_t *fota);
int fota_dbus_method_download(DBusMessage *msg, fota_server_t *fota);
int fota_dbus_method_restart(DBusMessage *msg, fota_server_t *fota);
int fota_dbus_method_size(DBusMessage *msg, fota_server_t *fota);

#ifdef __cplusplus
}
#endif

#endif /* __FOTA_SERVER_H__ */
