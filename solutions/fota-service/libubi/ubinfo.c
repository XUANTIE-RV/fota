/*
 * Copyright (C) 2007, 2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * An utility to get UBI information.
 *
 * Author: Artem Bityutskiy
 */

#define PROGRAM_NAME    "ubinfo"

#include <stdint.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <libubi.h>
#include <ulog/ulog.h>
#include "common.h"

#define TAG "ubinfo"

struct ubi_args_t {
	int devn;
	int vol_id;
	int all;
	const char *node;
	const char *vol_name;
};
static struct ubi_args_t ubi_args;

static int translate_dev(libubi_t libubi, const char *node)
{
	int err;

	err = ubi_probe_node(libubi, node);
	if (err == -1) {
		if (errno != ENODEV) {
            LOGE(TAG, "error while probing \"%s\"", node);
            return -1;
        }
        LOGE(TAG, "\"%s\" does not correspond to any UBI device or volume", node);
		return -1;
	}

	if (err == 1) {
		struct ubi_dev_info dev_info;

		err = ubi_get_dev_info(libubi, node, &dev_info);
		if (err) {
            LOGE(TAG, "cannot get information about UBI device \"%s\"", node);
            return -1;
        }
		ubi_args.devn = dev_info.dev_num;
	} else {
		struct ubi_vol_info vol_info;

		err = ubi_get_vol_info(libubi, node, &vol_info);
		if (err) {
            LOGE(TAG, "cannot get information about UBI volume \"%s\"", node);
            return -1;
        }
		if (ubi_args.vol_id != -1) {
            LOGE(TAG, "both volume character device node (\"%s\") and "
				      "volume ID (%d) are specify, use only one of them"
				      "(use -h for help)", node, ubi_args.vol_id);
            return -1;
        }
		ubi_args.devn = vol_info.dev_num;
		ubi_args.vol_id = vol_info.vol_id;
	}

	return 0;
}

static int get_vol_id_by_name(libubi_t libubi, int dev_num, const char *name)
{
	int err;
	struct ubi_vol_info vol_info;

	err = ubi_get_vol_info1_nm(libubi, dev_num, name, &vol_info);
	if (err) {
        LOGE(TAG, "cannot get information about volume \"%s\" on ubi%d\n", name, dev_num);
        return -1;
    }
	ubi_args.vol_id = vol_info.vol_id;

	return 0;
}

int get_ubi_info(const char *ubi_name, long long *bytes)
{
	int err;
	libubi_t libubi;

    ubi_args.vol_id = -1;
    ubi_args.devn = -1;
    ubi_args.all = 0;
    ubi_args.node = NULL;
    ubi_args.vol_name = NULL;

	libubi = libubi_open();
	if (!libubi) {
		if (errno == 0) {
            LOGE(TAG, "UBI is not present in the system");
            return -1;
        }
        LOGE(TAG, "cannot open libubi");
        return -1;
	}
    err = translate_dev(libubi, ubi_name);
    if (err) {
        goto out_libubi;
    }
	if (ubi_args.vol_name && ubi_args.devn == -1) {
		LOGE(TAG, "volume name is specified, but UBI device number is not");
		goto out_libubi;
	}

	if (ubi_args.vol_name) {
		err = get_vol_id_by_name(libubi, ubi_args.devn, ubi_args.vol_name);
		if (err)
			goto out_libubi;
	}

	if (ubi_args.vol_id != -1 && ubi_args.devn == -1) {
		LOGE(TAG, "volume ID is specified, but UBI device number is not");
		goto out_libubi;
	}

	if (ubi_args.devn != -1 && ubi_args.vol_id != -1) {
    	struct ubi_vol_info vol_info;
        err = ubi_get_vol_info1(libubi, ubi_args.devn, ubi_args.vol_id, &vol_info);
        if (err) {
            LOGE(TAG, "cannot get information about UBI volume %d on ubi%d", ubi_args.vol_id, ubi_args.devn);
            goto out_libubi;
        }
        *bytes = vol_info.rsvd_bytes;
        LOGD(TAG, "vol_info.rsvd_bytes:%lld", vol_info.rsvd_bytes);
		goto out;
	}

	if (err)
		goto out_libubi;
out:
	libubi_close(libubi);
	return 0;

out_libubi:
	libubi_close(libubi);
	return -1;
}
