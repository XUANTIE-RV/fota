/*
 * Backtrace debugging
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef TRACE_H
#define TRACE_H

#define FOTA_TRACE_LEN 16

#ifdef FOTA_TRACE
#include <execinfo.h>

#include "list.h"

#define FOTA_TRACE_INFO void *btrace[FOTA_TRACE_LEN]; int btrace_num;

struct fota_trace_ref {
    struct dl_list list;
    const void *addr;
    FOTA_TRACE_INFO
};
#define FOTA_TRACE_REF(name) struct fota_trace_ref fota_trace_ref_##name

#define fota_trace_dump(title, ptr) \
    fota_trace_dump_func((title), (ptr)->btrace, (ptr)->btrace_num)
void fota_trace_dump_func(const char *title, void **btrace, int btrace_num);
#define fota_trace_record(ptr) \
    (ptr)->btrace_num = backtrace((ptr)->btrace, FOTA_TRACE_LEN)
void fota_trace_show(const char *title);
#define fota_trace_add_ref(ptr, name, addr) \
    fota_trace_add_ref_func(&(ptr)->fota_trace_ref_##name, (addr))
void fota_trace_add_ref_func(struct fota_trace_ref *ref, const void *addr);
#define fota_trace_remove_ref(ptr, name, addr)	\
    do { \
        if ((addr)) \
            dl_list_del(&(ptr)->fota_trace_ref_##name.list); \
    } while (0)
void fota_trace_check_ref(const void *addr);
size_t fota_trace_calling_func(const char *buf[], size_t len);

#else /* FOTA_TRACE */

#define FOTA_TRACE_INFO
#define FOTA_TRACE_REF(n)
#define fota_trace_dump(title, ptr) do { } while (0)
#define fota_trace_record(ptr) do { } while (0)
#define fota_trace_show(title) do { } while (0)
#define fota_trace_add_ref(ptr, name, addr) do { } while (0)
#define fota_trace_remove_ref(ptr, name, addr) do { } while (0)
#define fota_trace_check_ref(addr) do { } while (0)

#endif /* FOTA_TRACE */


#ifdef FOTA_TRACE_BFD

void fota_trace_dump_funcname(const char *title, void *pc);

#else /* FOTA_TRACE_BFD */

#define fota_trace_dump_funcname(title, pc) do { } while (0)

#endif /* FOTA_TRACE_BFD */

void fota_trace_deinit(void);

#endif /* TRACE_H */
