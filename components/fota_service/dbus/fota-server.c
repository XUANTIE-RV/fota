/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

#include <dbus/dbus.h>

#include <dbus_new_helpers.h>

#include <fota.h>
#include <fota_debug.h>

static int should_dispatch;
struct fota_dbus_object_desc obj_dsc;

int fota_dbus_signal_version(fota_server_t *fota, const char *str)
{
    DBusMessage *msg;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    char *_str = (char *)str;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    msg = dbus_message_new_signal(FOTA_DBUS_PATH,
                                  FOTA_DBUS_INTERFACE, FOTA_DBUS_SIGNAL_VERSION);

    if (NULL == msg) {
        fota_log(LOG_ERR, "Message Null\n");
        return -1;
    }

    dbus_message_iter_init_append(msg, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &_str)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, msg, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(msg);

    return 0;
}

int fota_dbus_signal_download(fota_server_t *fota, const char *str)
{
    DBusMessage *msg;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    char *_str = (char *)str;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    msg = dbus_message_new_signal(FOTA_DBUS_PATH,
                                  FOTA_DBUS_INTERFACE, FOTA_DBUS_SIGNAL_DOWNLOAD);

    if (NULL == msg) {
        fota_log(LOG_ERR, "Message Null\n");
        return -1;
    }

    dbus_message_iter_init_append(msg, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &_str)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, msg, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(msg);

    return 0;
}

int fota_dbus_signal_end(fota_server_t *fota, const char *str)
{
    DBusMessage *msg;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    char *_str = (char *)str;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    msg = dbus_message_new_signal(FOTA_DBUS_PATH,
                                  FOTA_DBUS_INTERFACE, FOTA_DBUS_SIGNAL_END);

    if (NULL == msg) {
        fota_log(LOG_ERR, "Message Null\n");
        return -1;
    }

    dbus_message_iter_init_append(msg, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &_str)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, msg, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(msg);

    return 0;
}

int fota_dbus_signal_restart(fota_server_t *fota, const char *str)
{
    DBusMessage *msg;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    char *_str = (char *)str;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    msg = dbus_message_new_signal(FOTA_DBUS_PATH,
                                  FOTA_DBUS_INTERFACE, FOTA_DBUS_SIGNAL_RESTART);

    if (NULL == msg) {
        fota_log(LOG_ERR, "Message Null\n");
        return -1;
    }

    dbus_message_iter_init_append(msg, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &_str)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, msg, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(msg);

    return 0;
}

void fotax_event(fotax_t *fotax, fotax_event_e event, const char *json)
{
    fota_server_t *fota_server = (fota_server_t *)fotax->private;

    switch (event) {
        case FOTAX_EVENT_VERSION:
            fota_log(LOG_DEBUG, "%s,%d,%s\n", __func__, __LINE__, json);
            fota_dbus_signal_version(fota_server, json);
            break;

        case FOTAX_EVENT_DOWNLOAD:
            fota_log(LOG_DEBUG, "%s,%d,%s\n", __func__, __LINE__, json);
            fota_dbus_signal_download(fota_server, json);
            break;

        case FOTAX_EVENT_END:
            fota_log(LOG_DEBUG, "%s,%d,%s\n", __func__, __LINE__, json);
            fota_dbus_signal_end(fota_server, json);
            break;

        case FOTAX_EVENT_RESTART:
            fota_log(LOG_DEBUG, "%s,%d,%s\n", __func__, __LINE__, json);
            fota_dbus_signal_restart(fota_server, json);
            break;

        default: break;
    }
}

int fota_dbus_method_start(DBusMessage *msg, fota_server_t *fota)
{
    int ret_val;
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    ret_val = fotax_start(&fota->fotax);
    if (ret_val == 0) {
        fota->fotax.fotax_event_cb = fotax_event;
        fota->fotax.private = fota;
    }

    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret_val)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

int fota_dbus_method_stop(DBusMessage *msg, fota_server_t *fota)
{
    int ret_val;
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);
    ret_val = fotax_stop(&fota->fotax);

    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret_val)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

int fota_dbus_method_get_state(DBusMessage *msg, fota_server_t *fota)
{
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    char ret_str[10];
    char *str = (char *)&ret_str;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);
    memset(&ret_str, 0, sizeof(ret_str));
    int ret = fotax_get_state(&fota->fotax);
    // 版本检测，下载状态，停止状态，空闲状态
    if (ret == FOTAX_INIT) {
        strncpy(ret_str, "idle", sizeof(ret_str) - 1);
    } else if (ret == FOTAX_DOWNLOAD ) {
        strncpy(ret_str, "download", sizeof(ret_str) - 1);
    } else if (ret == FOTAX_ABORT ) {
        strncpy(ret_str, "abort", sizeof(ret_str) - 1);
    } else if (ret == FOTAX_FINISH ) {
        strncpy(ret_str, "finish", sizeof(ret_str) - 1);
    } else {
        strncpy(ret_str, "", sizeof(ret_str) - 1);
    }

    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

int fota_dbus_method_version_check(DBusMessage *msg, fota_server_t *fota)
{
    int ret_val;
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);
    ret_val = fotax_version_check(&fota->fotax);
    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);
    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret_val)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

int fota_dbus_method_download(DBusMessage *msg, fota_server_t *fota)
{
    int ret_val;
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);
    ret_val = fotax_download(&fota->fotax);
    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);
    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret_val)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

int fota_dbus_method_restart(DBusMessage *msg, fota_server_t *fota)
{
    int ret_val;
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    double _millisecond;
    int millisecond;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    dbus_message_iter_init(msg, &iter);

//    printf("type = %d\n", dbus_message_iter_get_arg_type(&iter));
//    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INT32) {
//        fota_log(LOG_ERR, "not int\n");
//    }

    dbus_message_iter_get_basic(&iter, &_millisecond);
    millisecond = (int)_millisecond;

    printf("millisecond = %d\n", millisecond);
    ret_val = fotax_restart(&fota->fotax, millisecond);

    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &ret_val)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

int fota_dbus_method_size(DBusMessage *msg, fota_server_t *fota)
{
    int64_t ret_val;
    DBusMessage *reply;
    DBusMessageIter iter;
    dbus_uint32_t serial = 0;
    DBusConnection *conn = fota->conn;
    char *name;

    fota_log(LOG_DEBUG, "Enter %s\n", __func__);

    dbus_message_iter_init(msg, &iter);

    dbus_message_iter_get_basic(&iter, &name);

    printf("name = %s\n", name);
    ret_val = fotax_get_size(&fota->fotax, name);

    reply = dbus_message_new_method_return(msg);

    dbus_message_iter_init_append(reply, &iter);

    if (!dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT64, &ret_val)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    if (!dbus_connection_send(conn, reply, &serial)) {
        fota_log(LOG_ERR, "Out Of Memory!\n");
        return -1;
    }

    dbus_connection_flush(conn);

    dbus_message_unref(reply);

    return 0;
}

static void msg_method_handler(DBusMessage *msg, fota_server_t *fota)
{
    const char *member;

    fota_log(LOG_INFO, "Enter %s\n", __func__);

    member = dbus_message_get_member(msg);

    fota_log(LOG_INFO, "method call: %s\n", member);

    for (int i = 0; ; i++) {
        if (fota_dbus_global_methods[i].dbus_method == NULL) {
            fota_log(LOG_ERR, "not method call\n");
            break;
        }

        if (!strcmp(fota_dbus_global_methods[i].dbus_method, member)) {
            fota_dbus_global_methods[i].function(msg, fota);
            break;
        }
    }
}

static DBusHandlerResult message_handler(DBusConnection *connection,
                                         DBusMessage *message, void *user_data)
{
    const char *method;
    const char *path;
    const char *interface;

    method = dbus_message_get_member(message);
    path = dbus_message_get_path(message);
    interface = dbus_message_get_interface(message);

    if (!method || !path || !interface) {
        return -1;
    }

    if (!strncmp(FOTA_DBUS_INTROSPECTION_METHOD, method, 50) &&
        !strncmp(FOTA_DBUS_INTROSPECTION_INTERFACE, interface, 50)) {
        fota_dbus_introspect(message, &obj_dsc, (fota_server_t *)user_data);
    } else if(!strncmp(FOTA_DBUS_INTERFACE, interface, 50)) {
        msg_method_handler(message, (fota_server_t *)user_data);
    } else {
        fota_log(LOG_ERR, "message error\n");
    }

    return DBUS_HANDLER_RESULT_HANDLED;
}

int fota_dbus_ctrl_iface_init(fota_server_t *fota)
{
    DBusObjectPathVTable vtable = {
        NULL, &message_handler,
        NULL, NULL, NULL, NULL
    };

    if (!dbus_connection_register_object_path(fota->conn, FOTA_DBUS_PATH, &vtable, fota)) {
        fota_log(LOG_ERR, "dbus_connection_register_object_path error\n");
        return -1;
    }

    return 0;
}

static dbus_bool_t add_watch(DBusWatch *watch, void *data)
{
    int fd;
    unsigned int flags;
    fota_server_t *fota = (fota_server_t *)data;

    if (!dbus_watch_get_enabled(watch)) {
        return TRUE;
    }

    flags = dbus_watch_get_flags(watch);
    fd = dbus_watch_get_unix_fd(watch);

    if (flags & DBUS_WATCH_READABLE) {
        fota->watch_rfd = fd;
        fota->watch = watch;
    }

    return TRUE;
}

static void wakeup_main(void *data)
{
    should_dispatch = 1;
}


static int integrate_dbus_watch(fota_server_t *fota)
{
    if (!dbus_connection_set_watch_functions(fota->conn, add_watch, NULL, NULL, fota, NULL)) {
        fota_log(LOG_ERR, "dbus_connection_set_watch_functions error\n");
        return -1;
    }

    dbus_connection_set_wakeup_main_function(fota->conn, wakeup_main,
                                             fota, NULL);


    return 0;
}

static void dispatch_data(DBusConnection *con)
{
    while (dbus_connection_get_dispatch_status(con) ==
       DBUS_DISPATCH_DATA_REMAINS)
        dbus_connection_dispatch(con);
}

static void process_watch(fota_server_t *fota, DBusWatch *watch)
{
    dbus_connection_ref(fota->conn);

    should_dispatch = 0;

    dbus_watch_handle(watch, DBUS_WATCH_READABLE);

    if (should_dispatch) {
        dispatch_data(fota->conn);
        should_dispatch = 0;
    }

    dbus_connection_unref(fota->conn);
}

static void process_watch_read(fota_server_t *fota, DBusWatch *watch)
{
    process_watch(fota, watch);
}

void fota_loop_run(fota_server_t *fota)
{
    int    sockfd;
    fd_set rset;
    int    retval;

    integrate_dbus_watch(fota);

    sockfd = fota->watch_rfd;

    FD_ZERO(&rset);
    FD_SET(sockfd, &rset);

    while (1) {
        retval = select(sockfd + 1, &rset, NULL, NULL, NULL);

        if (retval == -1) {
            fota_log(LOG_ERR, "select error\n");
        } else if (retval) {
             process_watch_read(fota, fota->watch);
        } else {
            fota_log(LOG_ERR, "select timeout\n");
            // timeout
        }
    }
}

int fota_dbus_init(fota_server_t *fota)
{
    int ret;
    DBusError err;
    DBusConnection *conn;

    dbus_error_init(&err);

    conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

    obj_dsc.connection = conn;

    fota->conn = conn;

    fota_dbus_register_desc();

    fota_dbus_ctrl_iface_init(fota);

    if (dbus_error_is_set(&err)) {
        fota_log(LOG_ERR, "Connection Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    if (conn == NULL) {
        exit(1);
    }

    ret = dbus_bus_request_name(conn, FOTA_DBUS_SERVER, DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);

    if (dbus_error_is_set(&err)) {
        fota_log(LOG_ERR, "Name Error (%s)\n", err.message);
        dbus_error_free(&err);
    }

    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
	fota_log(LOG_ERR, "Error: Service has been started already\n");
        exit(1);
    }

    /////////////////////////////////////////////////////////////////////
    dbus_bus_add_match(conn,
                       "type='method_call',interface='org.fota.interface'",
                       &err);
    dbus_connection_flush(conn);

    if (dbus_error_is_set(&err)) {
        fota_log(LOG_ERR, "Match Error (%s)\n", err.message);
        exit(1);
    }

    return 0;
}

void fota_dbus_deinit(fota_server_t *fota)
{
    DBusError err;

    dbus_error_init(&err);

    dbus_bus_release_name(fota->conn, FOTA_DBUS_SERVER, &err);
}
