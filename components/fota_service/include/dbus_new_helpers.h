/*
 * FOTA Supplicant / dbus-based control interface
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009, Witold Sowa <witold.sowa@gmail.com>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef _DBUS_NEW_HELPER_H_
#define _DBUS_NEW_HELPER_H_

#include <stdint.h>
#include <dbus/dbus.h>
#include <fota.h>

typedef int (*method_function)(DBusMessage *message, fota_server_t *fota);
typedef void (*FOTADBusArgumentFreeFunction)(void *handler_arg);

struct fota_dbus_property_desc;
typedef dbus_bool_t (*FOTADBusPropertyAccessor)(
    const struct fota_dbus_property_desc *property_desc,
    DBusMessageIter *iter, DBusError *error, void *user_data);
#define DECLARE_ACCESSOR(f) \
    dbus_bool_t f(const struct fota_dbus_property_desc *property_desc, \
                  DBusMessageIter *iter, DBusError *error, void *user_data)

struct fota_dbus_object_desc {
    DBusConnection *connection;
    char *path;

    /* list of methods, properties and signals registered with object */
    const struct fota_dbus_method_desc *methods;
    const struct fota_dbus_signal_desc *signals;
    const struct fota_dbus_property_desc *properties;

    /* property changed flags */
    uint8_t *prop_changed_flags;

    /* argument for method handlers and properties
     * getter and setter functions */
    void *user_data;
    /* function used to free above argument */
    FOTADBusArgumentFreeFunction user_data_free_func;
};

enum dbus_arg_direction { ARG_IN, ARG_OUT };

/**
 * type Definition:
 *
 * 'Auto':  'v'
 * String:  's'
 * Number:  'd'
 * Boolean: 'b'
 * Array:   'av'
 * Object:  'a{sv}'
 */
struct fota_dbus_argument {
    char *name;
    char *type;
    enum dbus_arg_direction dir;
};

#define END_ARGS { NULL, NULL, ARG_IN }

/**
 * struct fota_dbus_method_desc - DBus method description
 */
struct fota_dbus_method_desc {
    /* method name */
    const char *dbus_method;
    /* method interface */
    const char *dbus_interface;
    /* method handling function */
    method_function function;
    /* array of arguments */
    struct fota_dbus_argument args[4];
};

/**
 * struct fota_dbus_signal_desc - DBus signal description
 */
struct fota_dbus_signal_desc {
    /* signal name */
    const char *dbus_signal;
    /* signal interface */
    const char *dbus_interface;
    /* array of arguments */
    struct fota_dbus_argument args[4];
};

/**
 * struct fota_dbus_property_desc - DBus property description
 */

struct fota_dbus_property_desc {
    /* property name */
    const char *dbus_property;
    /* property interface */
    const char *dbus_interface;
    /* property type signature in DBus type notation */
    const char *type;
    /* property getter function */
    FOTADBusPropertyAccessor getter;
    /* property setter function */
    FOTADBusPropertyAccessor setter;
    /* other data */
    const char *data;
};

#define FOTA_DBUS_INTROSPECTION_INTERFACE "org.freedesktop.DBus.Introspectable"
#define FOTA_DBUS_INTROSPECTION_METHOD "Introspect"
#define FOTA_DBUS_PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define FOTA_DBUS_PROPERTIES_GET "Get"
#define FOTA_DBUS_PROPERTIES_SET "Set"
#define FOTA_DBUS_PROPERTIES_GETALL "GetAll"

extern const struct fota_dbus_method_desc fota_dbus_global_methods[];
DBusMessage *fota_dbus_introspect(DBusMessage *message,
                                   struct fota_dbus_object_desc *obj_dsc,
                                   fota_server_t *fota);

#endif /* _DBUS_NEW_HELPER_H_ */
