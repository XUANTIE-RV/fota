/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#define _GNU_SOURCE
#include <pthread.h>
#include <semaphore.h>
#include "aos/list.h"

#include <aos/kernel.h>

#include <ulog/ulog.h>
#define TAG "TIMER"

#define CLOCKID CLOCK_MONOTONIC

// #include <aos/aos.h>

static timer_t s_timerid;
static dlist_t s_aos_timer_listhead;
static pthread_mutex_t s_list_mutex;
static sem_t     s_timer_sem;

typedef void (*timer_cb_t)(void *timer, void *args);

typedef struct timer_s {
    timer_cb_t     handler;
    void             *args;
    uint64_t          timeout;
    uint64_t          start_ms;
    int               repeat;  
} p_timer_t;

typedef struct timer_list {
    dlist_t node;
    p_timer_t *timer;
} timer_list_t ;

static void timer_callback(void *ptr) {
    sem_post(&s_timer_sem);
}

static uint64_t now(void)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_BOOTTIME, &ts) == -1) {
    LOGE(TAG, "%s unable to get current time: %s",
              __func__, strerror(errno));
    return 0;
  }

  return (ts.tv_sec * 1000LL) + (ts.tv_nsec / 1000000LL);
}

// static void start_timer(uint64_t timerout_ms)
// {
//     struct itimerspec wakeup_time;
//     memset(&wakeup_time, 0, sizeof(wakeup_time));

//     // LOGD(TAG, "%s timer start %d\n", __func__, timerout_ms);

//     wakeup_time.it_value.tv_sec = (timerout_ms / 1000);
//     wakeup_time.it_value.tv_nsec = (timerout_ms % 1000) * 1000000LL;

//     if (timer_settime(s_timerid, TIMER_ABSTIME, &wakeup_time, NULL) == -1)
//         LOGE(TAG, "%s unable to set timer: %s",
//                     __func__, strerror(errno));
// }

static void restart_timer(uint64_t timerout_ms)
{
    struct itimerspec wakeup_time;
    memset(&wakeup_time, 0, sizeof(wakeup_time));

    // LOGD(TAG, "%s timeout %d\n", __func__, timerout_ms);

    /* stop the timer */
    timer_settime(s_timerid, TIMER_ABSTIME, &wakeup_time, NULL);

    wakeup_time.it_value.tv_sec = (timerout_ms / 1000);
    wakeup_time.it_value.tv_nsec = (timerout_ms % 1000) * 1000000LL;

    if (timer_settime(s_timerid, TIMER_ABSTIME, &wakeup_time, NULL) == -1)
        LOGE(TAG, "%s unable to set timer: %s",
                    __func__, strerror(errno));
}

static void add_node_to_list(p_timer_t *timer)
{
    timer_list_t* node = malloc(sizeof(timer_list_t));
    timer_list_t* tmp_node;

    if (node == NULL) {
        LOGE(TAG, "OOM\n");
        return;
    }

    timer->start_ms = now() + timer->timeout;
    node->timer = timer;


    if (!dlist_empty(&s_aos_timer_listhead)) {
        dlist_for_each_entry(&s_aos_timer_listhead, tmp_node, timer_list_t, node) {
            if (tmp_node->timer->start_ms > timer->start_ms) {
                break;
            }
        }

        dlist_add_tail(&node->node, &tmp_node->node);
    } else {
        dlist_add_tail(&node->node, &s_aos_timer_listhead);
    }
}

static int is_in_list(p_timer_t *timer)
{
    timer_list_t* tmp_node;

    dlist_for_each_entry(&s_aos_timer_listhead, tmp_node, timer_list_t, node) {
        if (tmp_node->timer == timer) {
            return 1;
        }
    }

    return 0;
}

static void* timer_dispatch(void *args)
{
    while(1) {
        sem_wait(&s_timer_sem);

        // LOGD(TAG, "%s entry 1\n", __func__);

        if (dlist_empty(&s_aos_timer_listhead)) {
            continue;
        }

        timer_list_t* tmp_node;

        // LOGD(TAG, "%s entry 2\n", __func__);

        pthread_mutex_lock(&s_list_mutex);

        while (1) {
            int del = 0;
            p_timer_t *tmp_timer;
            uint64_t timeout_ms = now();

            dlist_for_each_entry(&s_aos_timer_listhead, tmp_node, timer_list_t, node) {
                if (tmp_node->timer->start_ms <= timeout_ms) {
                    tmp_timer = tmp_node->timer;
                    tmp_node->timer->handler(tmp_node->timer, tmp_node->timer->args);
                    // LOGD(TAG, "%s handler\n", __func__);
                    del = 1;
                    break;
                }
            }

            if (del == 0) {
                break;
            } else {
                /* check the list for avoid call aos_timer_stop in timer->handle */
                if (is_in_list(tmp_timer) && tmp_timer->start_ms <= timeout_ms) {
                    dlist_del(&tmp_node->node);

                    if (tmp_timer->repeat == 1) {
                        add_node_to_list(tmp_timer);
                    }
                    free(tmp_node);
                }
            }
        }

        if (!dlist_empty(&s_aos_timer_listhead)) {
            tmp_node = dlist_first_entry(&s_aos_timer_listhead, timer_list_t, node);

            // LOGD(TAG, "%s timer start %p\n", __func__, tmp_node->timer);

            restart_timer(tmp_node->timer->start_ms);
        }

        pthread_mutex_unlock(&s_list_mutex);
    }

    return NULL;
}

static void lazy_init()
{
    static int init = 0;

    if (init == 0) {
        struct sigevent sigevent;

        memset(&sigevent, 0, sizeof(sigevent));
        sigevent.sigev_notify = SIGEV_THREAD;
        sigevent.sigev_notify_function = (void (*)(union sigval))timer_callback;

        if (timer_create(CLOCKID, &sigevent, &s_timerid) == -1) {
            LOGE(TAG, "%s unable to create timer with clock %d: %s\n", __func__, CLOCKID, strerror(errno));
            assert(NULL);
        }

        dlist_init(&s_aos_timer_listhead);

        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);

        pthread_mutex_init(&s_list_mutex, &attr);

        sem_init(&s_timer_sem, 0, 0);

        pthread_t tid = 0;

        pthread_create(&tid, NULL, timer_dispatch, NULL);

        pthread_setname_np(tid, "timer_task");
    }

    init = 1;
}

int aos_timer_new(aos_timer_t *timer, void (*fn)(void *, void *),
                  void *arg, int ms, int repeat)
{
    lazy_init();

    p_timer_t *t = malloc(sizeof(p_timer_t));

    if (t == NULL) {
        LOGE(TAG, "%s OOM", __func__);
        return -1;
    }

    t->handler  = fn;
    t->args     = arg;
    t->start_ms = 0;
    t->repeat   = repeat;
    t->timeout  = ms;

    timer->hdl = t;

    return 0;
}

int aos_timer_new_ext(aos_timer_t *timer, void (*fn)(void *, void *),
                      void *arg, int ms, int repeat, unsigned char auto_run)
{
    aos_timer_new(timer, fn, arg, ms, repeat);

    if (auto_run == 1) {
        return aos_timer_start(timer);
    } else {
        return 0;
    }
}

int aos_timer_start(aos_timer_t *timer)
{
    timer_list_t* tmp_node;

    assert(timer);

    p_timer_t *t = timer->hdl;
    // LOGD(TAG, "%s timer %p,timeout %u\n", __func__, timer, timeout);

    t->start_ms = now() + t->timeout;

    timer_list_t* node = malloc(sizeof(timer_list_t));

    if (node == NULL) {
        LOGE(TAG, "OOM\n");
        return -1;
    }

    node->timer = t;

    pthread_mutex_lock(&s_list_mutex);

    /* rm the timer if the timer is in the list */
    dlist_for_each_entry(&s_aos_timer_listhead, tmp_node, timer_list_t, node) {
        if (tmp_node->timer == t) {
            dlist_del(&tmp_node->node);
            free(tmp_node);
            break;
        }
    }

    if (!dlist_empty(&s_aos_timer_listhead)) {
        int first = 0;
        dlist_for_each_entry(&s_aos_timer_listhead, tmp_node, timer_list_t, node) {
            if (tmp_node->timer->start_ms > t->start_ms) {
                first ++;
                break;
            }
        }

        dlist_add_tail(&node->node, &tmp_node->node);

        /* if the timer is earliest, restart posix timer */
        if (first == 1) {
            restart_timer(t->start_ms);
        }
    } else {
        dlist_add_tail(&node->node, &s_aos_timer_listhead);

        restart_timer(node->timer->start_ms);
    }

    pthread_mutex_unlock(&s_list_mutex);

    return 0;
}

int aos_timer_stop(aos_timer_t *timer)
{
    timer_list_t* tmp_node;
    assert(timer);
    p_timer_t *t = timer->hdl;

    pthread_mutex_lock(&s_list_mutex);

    /* rm the timer if the timer is in the list */
    dlist_for_each_entry(&s_aos_timer_listhead, tmp_node, timer_list_t, node) {
        if (tmp_node->timer == t) {
            dlist_del(&tmp_node->node);
            free(tmp_node);
            break;
        }
    }

    pthread_mutex_unlock(&s_list_mutex);
    return 0;
}

void aos_timer_free(aos_timer_t *timer)
{
    free(timer->hdl);
}

int aos_timer_change(aos_timer_t *timer, int ms)
{
    assert(timer);

    p_timer_t *t = timer->hdl;

    pthread_mutex_lock(&s_list_mutex);
    t->timeout  = ms;
    pthread_mutex_unlock(&s_list_mutex);

    return aos_timer_start(timer);
}

int aos_timer_change_once(aos_timer_t *timer, int ms)
{
    assert(timer);

    p_timer_t *t = timer->hdl;

    pthread_mutex_lock(&s_list_mutex);
    t->repeat = 0;
    t->timeout  = ms;
    pthread_mutex_unlock(&s_list_mutex);

    return aos_timer_start(timer);
}

int aos_timer_is_valid(aos_timer_t *timer)
{
    p_timer_t *t;

    if (timer == NULL) {
        return 0;
    }

    t = timer->hdl;
    if (t == NULL) {
        return 0;
    }

    if (t->handler == NULL) {
        return 0;
    }

    return 1;
}
