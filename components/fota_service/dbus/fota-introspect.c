/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

/*
 * fota_supplicant - D-Bus introspection
 * Copyright (c) 2006, Dan Williams <dcbw@redhat.com> and Red Hat, Inc.
 * Copyright (c) 2009, Witold Sowa <witold.sowa@gmail.com>
 * Copyright (c) 2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/list.h"
#include "utils/fota_buf.h"
#include "dbus_new_helpers.h"

#include <fota.h>

#define fota_printf(...)

struct interfaces {
    struct dl_list list;
    char *dbus_interface;
    struct fotabuf *xml;
};

static void *zalloc(size_t size)
{
    void *n = malloc(size);

    if (n) {
        memset(n, 0, size);
    }

    return n;
}

static struct interfaces *add_interface(struct dl_list *list,
                                        const char *dbus_interface)
{
    struct interfaces *iface;

    dl_list_for_each(iface, list, struct interfaces, list) {
        if (os_strcmp(iface->dbus_interface, dbus_interface) == 0) {
            return iface; /* already in the list */
        }
    }

    iface = zalloc(sizeof(struct interfaces));

    if (!iface) {
        return NULL;
    }

    iface->dbus_interface = os_strdup(dbus_interface);
    iface->xml = fotabuf_alloc(15000);

    if (iface->dbus_interface == NULL || iface->xml == NULL) {
        os_free(iface->dbus_interface);
        fotabuf_free(iface->xml);
        os_free(iface);
        return NULL;
    }

    fotabuf_printf(iface->xml, "<interface name=\"%s\">", dbus_interface);
    dl_list_add_tail(list, &iface->list);
    return iface;
}


static void add_arg(struct fotabuf *xml, const char *name, const char *type,
                    const char *direction)
{
    fotabuf_printf(xml, "<arg name=\"%s\"", name);

    if (type) {
        fotabuf_printf(xml, " type=\"%s\"", type);
    }

    if (direction) {
        fotabuf_printf(xml, " direction=\"%s\"", direction);
    }

    fotabuf_put_str(xml, "/>");
}


static void add_entry(struct fotabuf *xml, const char *type, const char *name,
                      const struct fota_dbus_argument *args, int include_dir)
{
    const struct fota_dbus_argument *arg;

    if (args == NULL || args->name == NULL) {
        fotabuf_printf(xml, "<%s name=\"%s\"/>", type, name);
        return;
    }

    fotabuf_printf(xml, "<%s name=\"%s\">", type, name);

    for (arg = args; arg && arg->name; arg++) {
        add_arg(xml, arg->name, arg->type,
                include_dir ? (arg->dir == ARG_IN ? "in" : "out") :
                NULL);
    }

    fotabuf_printf(xml, "</%s>", type);
}


static void add_property(struct fotabuf *xml,
                         const struct fota_dbus_property_desc *dsc)
{
    fotabuf_printf(xml, "<property name=\"%s\" type=\"%s\" "
                    "access=\"%s%s\"/>",
                    dsc->dbus_property, dsc->type,
                    dsc->getter ? "read" : "",
                    dsc->setter ? "write" : "");
}


static void extract_interfaces_methods(
    struct dl_list *list, const struct fota_dbus_method_desc *methods)
{
    const struct fota_dbus_method_desc *dsc;
    struct interfaces *iface;

    for (dsc = methods; dsc && dsc->dbus_method; dsc++) {
        iface = add_interface(list, dsc->dbus_interface);

        if (iface) {
            add_entry(iface->xml, "method", dsc->dbus_method,
                      dsc->args, 1);
        }
    }
}


static void extract_interfaces_signals(
    struct dl_list *list, const struct fota_dbus_signal_desc *signals)
{
    const struct fota_dbus_signal_desc *dsc;
    struct interfaces *iface;

    for (dsc = signals; dsc && dsc->dbus_signal; dsc++) {
        iface = add_interface(list, dsc->dbus_interface);

        if (iface)
            add_entry(iface->xml, "signal", dsc->dbus_signal,
                      dsc->args, 0);
    }
}


static void extract_interfaces_properties(
    struct dl_list *list, const struct fota_dbus_property_desc *properties)
{
    const struct fota_dbus_property_desc *dsc;
    struct interfaces *iface;

    for (dsc = properties; dsc && dsc->dbus_property; dsc++) {
        iface = add_interface(list, dsc->dbus_interface);

        if (iface) {
            add_property(iface->xml, dsc);
        }
    }
}


/**
 * extract_interfaces - Extract interfaces from methods, signals and props
 * @list: Interface list to be filled
 * @obj_dsc: Description of object from which interfaces will be extracted
 *
 * Iterates over all methods, signals, and properties registered with an
 * object and collects all declared DBus interfaces and create interfaces'
 * node in XML root node for each. Returned list elements contain interface
 * name and XML node of corresponding interface.
 */
static void extract_interfaces(struct dl_list *list,
                               struct fota_dbus_object_desc *obj_dsc)
{
    extract_interfaces_methods(list, obj_dsc->methods);
    extract_interfaces_signals(list, obj_dsc->signals);
    extract_interfaces_properties(list, obj_dsc->properties);
}


static void add_interfaces(struct dl_list *list, struct fotabuf *xml)
{
    struct interfaces *iface, *n;

    dl_list_for_each_safe(iface, n, list, struct interfaces, list) {
        if (fotabuf_len(iface->xml) + 20 < fotabuf_tailroom(xml)) {
            fotabuf_put_buf(xml, iface->xml);
            fotabuf_put_str(xml, "</interface>");
        } else {
            fota_printf(MSG_DEBUG,
                         "dbus: Not enough room for add_interfaces inspect data: tailroom %u, add %u",
                         (unsigned int) fotabuf_tailroom(xml),
                         (unsigned int) fotabuf_len(iface->xml));
        }

        dl_list_del(&iface->list);
        fotabuf_free(iface->xml);
        os_free(iface->dbus_interface);
        os_free(iface);
    }
}


static void add_child_nodes(struct fotabuf *xml, DBusConnection *con,
                            const char *path)
{
    char **children;
    int i;

    /* add child nodes to introspection tree */
    dbus_connection_list_registered(con, path, &children);

    for (i = 0; children[i]; i++) {
        fotabuf_printf(xml, "<node name=\"%s\"/>", children[i]);
    }

    dbus_free_string_array(children);
}


static void add_introspectable_interface(struct fotabuf *xml)
{
    fotabuf_printf(xml, "<interface name=\"%s\">"
                    "<method name=\"%s\">"
                    "<arg name=\"data\" type=\"s\" direction=\"out\"/>"
                    "</method>"
                    "</interface>",
                    FOTA_DBUS_INTROSPECTION_INTERFACE,
                    FOTA_DBUS_INTROSPECTION_METHOD);
}


static void add_properties_interface(struct fotabuf *xml)
{
    fotabuf_printf(xml, "<interface name=\"%s\">",
                    FOTA_DBUS_PROPERTIES_INTERFACE);

    fotabuf_printf(xml, "<method name=\"%s\">", FOTA_DBUS_PROPERTIES_GET);
    add_arg(xml, "interface", "s", "in");
    add_arg(xml, "propname", "s", "in");
    add_arg(xml, "value", "v", "out");
    fotabuf_put_str(xml, "</method>");

    fotabuf_printf(xml, "<method name=\"%s\">", FOTA_DBUS_PROPERTIES_GETALL);
    add_arg(xml, "interface", "s", "in");
    add_arg(xml, "props", "a{sv}", "out");
    fotabuf_put_str(xml, "</method>");

    fotabuf_printf(xml, "<method name=\"%s\">", FOTA_DBUS_PROPERTIES_SET);
    add_arg(xml, "interface", "s", "in");
    add_arg(xml, "propname", "s", "in");
    add_arg(xml, "value", "v", "in");
    fotabuf_put_str(xml, "</method>");

    fotabuf_put_str(xml, "</interface>");
}


static void add_fotas_interfaces(struct fotabuf *xml,
                                  struct fota_dbus_object_desc *obj_dsc)
{
    struct dl_list ifaces;

    dl_list_init(&ifaces);
    extract_interfaces(&ifaces, obj_dsc);
    add_interfaces(&ifaces, xml);
}


/**
 * fota_dbus_introspect - Responds for Introspect calls on object
 * @message: Message with Introspect call
 * @obj_dsc: Object description on which Introspect was called
 * Returns: Message with introspection result XML string as only argument
 *
 * Iterates over all methods, signals and properties registered with
 * object and generates introspection data for the object as XML string.
 */
DBusMessage *fota_dbus_introspect(DBusMessage *msg,
                                   struct fota_dbus_object_desc *obj_dsc,
                                   fota_server_t *fota)
{

    DBusMessage *reply;
    struct fotabuf *xml;
    DBusConnection *conn = fota->conn;

    xml = fotabuf_alloc(20000);

    if (xml == NULL) {
        return NULL;
    }

    fotabuf_put_str(xml, "<?xml version=\"1.0\"?>\n");
    fotabuf_put_str(xml, DBUS_INTROSPECT_1_0_XML_DOCTYPE_DECL_NODE);
    fotabuf_put_str(xml, "<node>");

    add_introspectable_interface(xml);
    add_properties_interface(xml);
    add_fotas_interfaces(xml, obj_dsc);
    add_child_nodes(xml, obj_dsc->connection,
                    dbus_message_get_path(msg));

    fotabuf_put_str(xml, "</node>\n");

    reply = dbus_message_new_method_return(msg);

    if (reply) {
        const char *intro_str = fotabuf_head(xml);

        dbus_message_append_args(reply, DBUS_TYPE_STRING, &intro_str,
                                 DBUS_TYPE_INVALID);

        if (!dbus_message_get_no_reply(msg)) {
            dbus_connection_send(conn, reply, NULL);
        }

        dbus_message_unref(reply);
        dbus_connection_flush(conn);
    }

    fotabuf_free(xml);

    return reply;
}
