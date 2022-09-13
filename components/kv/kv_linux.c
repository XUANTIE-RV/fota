#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <ulog/ulog.h>
#include "kv_linux.h"

#define TAG "kvlinux"


static int char2hex(char c, uint8_t *x)
{
	if (c >= '0' && c <= '9') {
		*x = c - '0';
	} else if (c >= 'a' && c <= 'f') {
		*x = c - 'a' + 10;
	} else if (c >= 'A' && c <= 'F') {
		*x = c - 'A' + 10;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int hex2char(uint8_t x, char *c)
{
	if (x <= 9) {
		*c = x + '0';
	} else  if (x >= 10 && x <= 15) {
		*c = x - 10 + 'a';
	} else {
		return -EINVAL;
	}

	return 0;
}

static size_t bin2hex(const uint8_t *buf, size_t buflen, char *hex, size_t hexlen)
{
	if ((hexlen + 1) < buflen * 2) {
		return 0;
	}

	for (size_t i = 0; i < buflen; i++) {
		if (hex2char(buf[i] >> 4, &hex[2 * i]) < 0) {
			return 0;
		}
		if (hex2char(buf[i] & 0xf, &hex[2 * i + 1]) < 0) {
			return 0;
		}
	}

	hex[2 * buflen] = '\0';
	return 2 * buflen;
}

static size_t hex2bin(const char *hex, size_t hexlen, uint8_t *buf, size_t buflen)
{
	uint8_t dec;

	if (buflen < hexlen / 2 + hexlen % 2) {
		return 0;
	}

	/* if hexlen is uneven, insert leading zero nibble */
	if (hexlen % 2) {
		if (char2hex(hex[0], &dec) < 0) {
			return 0;
		}
		buf[0] = dec;
		hex++;
		buf++;
	}

	/* regular hex conversion */
	for (size_t i = 0; i < hexlen / 2; i++) {
		if (char2hex(hex[2 * i], &dec) < 0) {
			return 0;
		}
		buf[i] = dec << 4;

		if (char2hex(hex[2 * i + 1], &dec) < 0) {
			return 0;
		}
		buf[i] += dec;
	}

	return hexlen / 2 + hexlen % 2;
}

static int mkdirpath(const char *path)
{
    int  ret;
    char str[KV_MAX_KEYPATH_LEN];
    int need_exit;

    if (path[0] != '/') {
        return -1;
    }

    char *ch = strchr(path, '/');

    need_exit = 0;
    while (ch && !need_exit) {
        ch = strchr(ch + 1, '/');
        if (ch == NULL) {
            if (path[strlen(path) - 1] == '/') {
                break;
            }
            ch = (char *)path + strlen(path);
            need_exit = 1;
        }
        snprintf(str, ch - path + 1, "%s", path);
        if (access(str, 0) != 0) {
            ret = mkdir(str, 0644);
            LOGD(TAG, "mkdir %s ret=%s", str, strerror(ret));
        }
    }

    return 0;
}

static int rmdirpath(const char *path)
{
    DIR *          dir;
    struct dirent *ptr;

    if ((dir = opendir(path)) == NULL) {
        LOGE(TAG, "%s: opendir error,%s", __FUNCTION__, strerror(errno));
        return -1;
    }

    while ((ptr = readdir(dir)) != NULL) {
        if (strcmp(ptr->d_name, ".") == 0 ||
            strcmp(ptr->d_name, "..") == 0) ///current dir OR parrent dir
            continue;
        else if (ptr->d_type == DT_REG) {
            
            char *hexkey = ptr->d_name;
            char key[KV_MAX_KEY_LEN * 2];
            size_t ret = hex2bin(hexkey, strlen(hexkey), (uint8_t *)key, sizeof(key));
            key[ret] = '\0';
            LOGD(TAG, "%s: remove key %s", __FUNCTION__, key);

            char keypath[KV_MAX_KEYPATH_LEN];
            snprintf(keypath, sizeof(keypath), "%s/%s", path, hexkey);
            remove(keypath);
        } else if (ptr->d_type == DT_DIR) {
            LOGW(TAG, "%s: dir(%s) in kv path", __FUNCTION__, ptr->d_name);
        } else {
            LOGW(TAG, "%s: unvalid file(%s) type(%d) in kv path", __FUNCTION__, ptr->d_name,
                 ptr->d_type);
        }
    }
    closedir(dir);

    return 0;
}

int kv_init(kv_t *kv, const char *path)
{
    if (kv == NULL) {
        LOGE(TAG, "%s: param error, path=%p", __FUNCTION__, path);
        return -1;
    }

    /* like mkdir -p */
    mkdirpath(path);

    /* copy path to kv obj */
    snprintf(kv->path, sizeof(kv->path), "%s", path);

    /* just seem as yoc */
    kv->handle = 1;

    return 0;
}

int kv_reset(kv_t *kv)
{
    if (kv == NULL) {
        LOGE(TAG, "%s: param error", __FUNCTION__);
        return -1;
    }

    rmdirpath(kv->path);

    return 0;
}

int kv_set(kv_t *kv, const char *key, void *value, int size)
{
    FILE *fd = NULL;

    if (kv == NULL || value == NULL || size == 0) {
        LOGE(TAG, "%s: param error, kv=%p value=%p size=%d", __FUNCTION__, key, value, size);
        return -1;
    }

    char hexkey[KV_MAX_KEY_LEN * 2];
    bin2hex((uint8_t *)key, strlen(key), hexkey, sizeof(hexkey));

    char keypath[KV_MAX_KEYPATH_LEN];
    snprintf(keypath, sizeof(keypath), "%s/%s", kv->path, hexkey);

    fd = fopen(keypath, "w");
    if (fd == NULL) {
        LOGE(TAG, "open %s file error %s", key, strerror(errno));
        return -1;
    }

    size_t wlen = fwrite(value, 1, size, fd);
    if (wlen != size) {
        LOGE(TAG, "write size error, write size=%d ret=%d", size, wlen);
        fclose(fd);
        return -1;
    }

    fflush(fd);
    fsync(fileno(fd));

    fclose(fd);
    return wlen;
}

int kv_get(kv_t *kv, const char *key, void *value, int size)
{
    FILE *fd = NULL;

    if (kv == NULL || value == NULL || size == 0) {
        LOGE(TAG, "%s: param error, kv=%p value=%p size=%d", __FUNCTION__, key, value, size);
        return -1;
    }

    char hexkey[KV_MAX_KEY_LEN * 2];
    bin2hex((uint8_t *)key, strlen(key), hexkey, sizeof(hexkey));

    char keypath[KV_MAX_KEYPATH_LEN];
    snprintf(keypath, sizeof(keypath), "%s/%s", kv->path, hexkey);

    fd = fopen(keypath, "rb");
    if (fd == NULL) {
        //LOGE(TAG, "open %s file error %s", key, strerror(errno));
        return -1;
    }

    size_t rlen = fread(value, 1, size, fd);

    fclose(fd);

    return rlen;
}

int kv_rm(kv_t *kv, const char *key)
{
    if (kv == NULL || key == NULL) {
        LOGE(TAG, "%s: param error, kv=%p key=%p", __FUNCTION__, kv, key);
        return -1;
    }

    char hexkey[KV_MAX_KEY_LEN * 2];
    bin2hex((uint8_t *)key, strlen(key), hexkey, sizeof(hexkey));

    char keypath[KV_MAX_KEYPATH_LEN];
    snprintf(keypath, sizeof(keypath), "%s/%s", kv->path, hexkey);

    /* remove key */
    int ret = remove(keypath);
    if (ret < 0) {
        LOGE(TAG, "%s: remove key error, key=%s ret=%d", __FUNCTION__, keypath, ret);
    }

    return ret;
}

void kv_iter(kv_t *kv, void (*func)(char *key, char *val, uint16_t val_size, void *arg), void *arg)
{
    DIR *          dir;
    struct dirent *ptr;

    if (kv == NULL || func == NULL) {
        LOGE(TAG, "%s: param error", __FUNCTION__);
        return;
    }

    if ((dir = opendir(kv->path)) == NULL) {
        LOGE(TAG, "%s: opendir error,%s", __FUNCTION__, strerror(errno));
        return;
    }

    while ((ptr = readdir(dir)) != NULL) {
        if (strcmp(ptr->d_name, ".") == 0 ||
            strcmp(ptr->d_name, "..") == 0) ///current dir OR parrent dir
            continue;
        else if (ptr->d_type == DT_REG) {

            void *value = malloc(KV_MAX_VALUE_LEN);
            char *hexkey   = ptr->d_name;

            char key[KV_MAX_KEY_LEN * 2];
            size_t ret = hex2bin(hexkey, strlen(hexkey), (uint8_t *)key, sizeof(key));
            key[ret] = '\0';

            //LOGD(TAG, "%s: iter key %s", __FUNCTION__, key);

            int   rlen  = kv_get(kv, key, value, KV_MAX_VALUE_LEN);
            func(key, value, rlen, arg);
            free(value);

        } else if (ptr->d_type == DT_DIR) {
            LOGW(TAG, "%s: dir(%s) in kv path", __FUNCTION__, ptr->d_name);
        } else {
            LOGW(TAG, "%s: unvalid file(%s) type(%d) in kv path", __FUNCTION__, ptr->d_name,
                 ptr->d_type);
        }
    }
    closedir(dir);
}

/********************************
 * MUTEX
 ********************************/
#include <sys/ipc.h>
#include <sys/shm.h>

typedef struct {
    pthread_mutex_t lock;
    int             ready;
} pthread_mutex_shm_t;

int aos_kv_mutex_new(aos_kv_mutex_t *mutex)
{
    /* generate share mm keyid */
    key_t shmkey = ftok(mutex->key_file, 0);
    LOGD(TAG, "shmkey=0x%x file=%s", shmkey, mutex->key_file);
    if (shmkey < 0) {
        LOGE(TAG, "%s: shmkey error", __FUNCTION__);
        return -1;
    }

    /* create or get share mm by keyid */
    int shmid = shmget(shmkey, sizeof(pthread_mutex_shm_t), 0666 | IPC_CREAT);
    if (shmid < 0) {
        LOGE(TAG, "%s: shmget error", __FUNCTION__);
        return -1;
    }

    /* get share mm addr by shmid */
    void *share_addr = shmat(shmid, (void *)0, 0);
    if (share_addr == NULL) {
        LOGE(TAG, "%s: shmat error", __FUNCTION__);
        shmctl(shmid, IPC_RMID, NULL);
        return -1;
    }

    /* shm stat check, firt ref, clean buffer */
    struct shmid_ds shm_stat;
    if (shmctl(shmid, IPC_STAT, &shm_stat) < 0) {
        LOGE(TAG, "%s: shmctl get stat error", __FUNCTION__);
        return -1;
    }

    if (shm_stat.shm_nattch == 1) {
        LOGD(TAG, "first process init shm buffer");
        memset(share_addr, 0, sizeof(pthread_mutex_shm_t));
    }

    /* create share mutex */
    pthread_mutex_shm_t *shm_mutex = (pthread_mutex_shm_t *)share_addr;

    if (shm_mutex->ready != 0x05050505) {
        pthread_mutexattr_t mutex_attr;
        pthread_mutexattr_init(&mutex_attr);
        pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&shm_mutex->lock, &mutex_attr);
        shm_mutex->ready = 0x05050505;
        LOGD(TAG, "%s: create share mutex shmid=%d", __FUNCTION__, shmid);
    } else {
        LOGD(TAG, "%s: get share mutex shmid=%d", __FUNCTION__, shmid);
    }

    /* save handle */
    mutex->hdl = shm_mutex;

    return 0;
}

void aos_kv_mutex_free(aos_kv_mutex_t *mutex)
{
    LOGE(TAG, "%s: no implement", __FUNCTION__);
}

int aos_kv_mutex_lock(aos_kv_mutex_t *mutex, unsigned int timeout)
{
    if (mutex->hdl == NULL) {
        LOGI(TAG, "%s: mutex no hdl", __FUNCTION__);
        return -1;
    }

    pthread_mutex_shm_t *shm_mutex = (pthread_mutex_shm_t *)mutex->hdl;

    return pthread_mutex_lock(&shm_mutex->lock);
}

int aos_kv_mutex_unlock(aos_kv_mutex_t *mutex)
{
    if (mutex->hdl == NULL) {
        LOGI(TAG, "%s: mutex no hdl", __FUNCTION__);
        return -1;
    }

    pthread_mutex_shm_t *shm_mutex = (pthread_mutex_shm_t *)mutex->hdl;

    return pthread_mutex_unlock(&shm_mutex->lock);
}
