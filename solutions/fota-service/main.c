/*
 * Copyright (C) 2018-2021 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <aos/kv.h>
#include <aos/nvram.h>
#include <aos/version.h>
#include <ulog/ulog.h>
#include <fota.h>
#include <fota_debug.h>
#include <fota_server.h>
#include "imagef.h"

#define TAG "main"

extern int daemonize;

fota_server_t *fota_init(void)
{
    fota_server_t *fota;

    fota = malloc(sizeof(fota_server_t));
    if (!fota) {
        return NULL;
    }

    memset(fota, 0, sizeof(fota_server_t));

    fota_dbus_init(fota);

    return fota;
}

void fota_deinit(fota_server_t *fota)
{
    fota_dbus_deinit(fota);

    free(fota);
}

static const char short_options[] = "hvd";

static const struct option long_options[] = {
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {"daemon", 0, 0, 'd'},
    {0, 0, 0, 0}
};

static void usage(char *cmd)
{
    printf(
           "Usage: %s [OPTION]\n"
           "\n"
           "-h, --help              help\n"
           "-v, --version           print current version\n"
           "-d, --daemon            run daemon in the background\n",
           cmd
          );
}

static void version(char *cmd)
{
    printf("%s: version " "0.0.1" " by T-Head\n", cmd);
}

static int run_daemon(void)
{
#ifdef __linux__
    if (daemon(0, 0)) {
        perror("daemon");
        return -1;
    }
#endif

    return 0;
}

extern int fota_register_cop2(void);
extern int netio_register_httpc(const char *cert);
extern int netio_register_flash2(void);
extern int check_rootfs_partition(void);
extern int set_fota_success_param(void);
extern int set_fota_update_version_ok(void);

const fota_register_ops_t register_ops = {
    .cert = NULL,
    .fota_register_cloud = fota_register_cop2,
    .netio_register_from = netio_register_httpc,
    .netio_register_to = netio_register_flash2
};

int main(int argc, char *argv[])
{
    int c;
    int option_index;

    while ((c = getopt_long(argc, argv, short_options, long_options, &option_index)) != -1) {
        switch (c) {
            case 'h':
                usage(argv[0]);
                return 0;
            case 'v':
                version(argv[0]);
                return 0;
            case 'd':
                if (run_daemon()) {
                    daemonize = 0;
                    exit(1);
                } else {
                    daemonize = 1;
                }
                break;
            default:
                printf("Try `%s --help' for more information.\n", argv[0]);
                return 1;
        }
    }

    printf("Service Init!\n");

    aos_kv_init(KV_PATH_DEFULT);
    nvram_init(NV_PATH_DEFUALT);
    int ret = set_fota_success_param();
    if (ret == 2) {
        set_fota_update_version_ok();
    } else if (ret == 1) {
        LOGD(TAG, "FOTA boot failed, rollback already...");
    }
    if (check_rootfs_partition() == 1) {
        LOGD(TAG, "############current_AB: A");
    } else {
        LOGD(TAG, "############current_AB: B");
    }
    LOGD(TAG, "############current_version: %s", aos_get_app_version());
    LOGD(TAG, "############current_changelog: %s", aos_get_changelog());
#ifdef UBI_NOT_SUPPORT_INTERRUPTED_UPDATE
    if (FILE_SYSTEM_IS_UBI()) {
        // because of UBI volume features, see ubi-user.h in linux kernel
        aos_kv_setint("fota_offset", 0);
    }
#endif

    fota_server_t *fota = fota_init();
    if (fota)
        fota->fotax.register_ops = (fota_register_ops_t *)&register_ops;
    
    int auto_check_en;
    if (aos_kv_getint("fota_autock", &auto_check_en) < 0) {
        auto_check_en = 0;
    }
    LOGD(TAG, "fota_autock: %d", auto_check_en);
    if (auto_check_en > 0) {
        extern void fotax_event(fotax_t *fotax, fotax_event_e event, const char *json);
        if (fotax_start(&fota->fotax) == 0) {
            LOGD(TAG, "fota auto start ok.");
            fota->fotax.fotax_event_cb = fotax_event;
            fota->fotax.private = fota;
        }
    }

    fota_loop_run(fota);

    return 0;
}
