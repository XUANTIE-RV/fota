/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#ifndef __FOTA_H__
#define __FOTA_H__

#include <dbus/dbus.h>
#include <fotax.h>

#define FOTA_DBUS_SERVER    "org.fota.server"
#define FOTA_DBUS_PATH      "/org/fota/path"
#define FOTA_DBUS_INTERFACE "org.fota.interface"

#define FOTA_DBUS_SIGNAL_VERSION              "version"   // 带字符串
#define FOTA_DBUS_SIGNAL_DOWNLOAD             "download"  // 带字符串
#define FOTA_DBUS_SIGNAL_END                  "end"       // 带字符串
#define FOTA_DBUS_SIGNAL_RESTART              "restart"   // 带字符串

#define FOTA_DBUS_METHOD_CALL_START           "start"
#define FOTA_DBUS_METHOD_CALL_STOP            "stop"
#define FOTA_DBUS_METHOD_CALL_GET_STATE       "getState"
#define FOTA_DBUS_METHOD_CALL_VERSION_CHECK   "versionCheck"
#define FOTA_DBUS_METHOD_CALL_DOWNLOAD        "download"
#define FOTA_DBUS_METHOD_CALL_RESTART         "restart"
#define FOTA_DBUS_METHOD_CALL_SIZE            "availableSize"

typedef struct fota {
    DBusConnection *conn;      /* DBus connection handle */
    int             watch_rfd;
    void           *watch;
    fotax_t fotax;
    void *priv;
} fota_server_t;

fota_server_t *fota_init(void);
void fota_deinit(fota_server_t *fota);
int fota_dbus_init(fota_server_t *fota);
void fota_dbus_deinit(fota_server_t *fota);
void fota_dbus_register_desc(void);
void fota_loop_run(fota_server_t *fota);

#endif
