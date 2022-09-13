/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#include <dbus_new_helpers.h>
#include <fota_server.h>
#include <fota.h>

extern struct fota_dbus_object_desc obj_dsc;

const struct fota_dbus_method_desc fota_dbus_global_methods[] = {
    {
        FOTA_DBUS_METHOD_CALL_START, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_start,
        {
            END_ARGS
        }
    },
    {
        FOTA_DBUS_METHOD_CALL_STOP, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_stop,
        {
            END_ARGS
        }
    },
    {
        FOTA_DBUS_METHOD_CALL_GET_STATE, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_get_state,
        {
            END_ARGS
        }
    },
    {
        FOTA_DBUS_METHOD_CALL_VERSION_CHECK, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_version_check,
        {
            END_ARGS
        }
    },
    {
        FOTA_DBUS_METHOD_CALL_DOWNLOAD, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_download,
        {
            END_ARGS
        }
    },
    {
        FOTA_DBUS_METHOD_CALL_RESTART, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_restart,
        {
            { "sec", "d", ARG_IN },
            END_ARGS
        }
    },
    {
        FOTA_DBUS_METHOD_CALL_SIZE, FOTA_DBUS_INTERFACE,
        (method_function) fota_dbus_method_size,
        {
            { "name", "s", ARG_IN },
            END_ARGS
        }
    },
    { NULL, NULL, NULL, { END_ARGS } }
};

static const struct fota_dbus_signal_desc fota_dbus_global_signals[] = {
    {
        FOTA_DBUS_SIGNAL_VERSION, FOTA_DBUS_INTERFACE,
        {
            { "str", "s", ARG_OUT },
            END_ARGS
        }
    },
    {
        FOTA_DBUS_SIGNAL_DOWNLOAD, FOTA_DBUS_INTERFACE,
        {
            {"str", "s", ARG_OUT},
            END_ARGS
        }
    },
    {
        FOTA_DBUS_SIGNAL_END, FOTA_DBUS_INTERFACE,
        {
            {"str", "s", ARG_OUT},
            END_ARGS
        }
    },
    {
        FOTA_DBUS_SIGNAL_RESTART, FOTA_DBUS_INTERFACE,
        {
            {"str", "s", ARG_OUT},
            END_ARGS
        }
    },
    { NULL, NULL, { END_ARGS } }

};

void fota_dbus_register_desc(void)
{
    obj_dsc.methods = fota_dbus_global_methods;
    obj_dsc.signals = fota_dbus_global_signals;
}
