/*
 * Copyright (C) 2019-2020 Alibaba Group Holding Limited
 */
#ifndef __FOTAX_H__
#define __FOTAX_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FOTAX_EVENT_VERSION = 0,     /*!< Check version from server ok */
    FOTAX_EVENT_DOWNLOAD,        /*!< Downloading the fota data */
    FOTAX_EVENT_END,             /*!< This event occurs when there are any errors during execution */
    FOTAX_EVENT_RESTART          /*!< real want to restart */
} fotax_event_e;

typedef enum {
    FOTAX_INIT = 1,          /*!< create fota task, wait for version check */
    FOTAX_DOWNLOAD = 2,      /*!< start to download fota data */
    FOTAX_ABORT = 3,         /*!< read or write exception */
    FOTAX_FINISH = 4,        /*!< download finish */
} fotax_status_e;

typedef struct {
    void *cert;
    int (*fota_register_cloud)(void);
    int (*netio_register_from)(const char *cert);
    int (*netio_register_to)(void);
} fota_register_ops_t;

typedef struct fotax fotax_t;

typedef void (*fotax_event_cb_t)(fotax_t *fotax, fotax_event_e event, const char *json);   ///< fota Event call back.

struct fotax {
    void *fota_handle;
    fotax_status_e state;
    fotax_event_cb_t fotax_event_cb;
    fota_register_ops_t *register_ops;
    void *private;
};

int fotax_start(fotax_t *fotax);

int fotax_stop(fotax_t *fotax);

int fotax_get_state(fotax_t *fotax);

int fotax_version_check(fotax_t *fotax);

int fotax_download(fotax_t *fotax);

int fotax_restart(fotax_t *fotax, int delay_ms);

int64_t fotax_get_size(fotax_t *fotax, const char *name);

#ifdef __cplusplus
}
#endif
#endif