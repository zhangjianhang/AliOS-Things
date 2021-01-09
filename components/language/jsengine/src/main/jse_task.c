/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

/* this file is the JSEngine message/event process framework */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "jse_common.h"

#define JSE_MAIN_TASK_TICK (500)
#define JSE_MSGQ_WAITIME (2000)
#define JSE_MSGQ_MAX_NUM 64
#define JSE_TASK_MUTEX_WAITIME 5000

static osMessageQId jse_task_mq = NULL; /* JSEngine message queue */
static void *jse_task_mutex     = NULL; /* JSEngine mutex */

typedef struct {
    bone_engine_call_t action;
    void *arg;
} shedule_timer_msg_t;

int32_t jsengine_task_yield(uint32_t timeout)
{
    int32_t ret = 0;
    jse_task_msg_t jse_msg;

    if ((ret = jse_osal_messageQ_get(jse_task_mq, &jse_msg, timeout)) != 0)
        return -1;

    if (jse_msg.type == JSE_TASK_MSG_CALLBACK)
        jse_msg.callback(jse_msg.param);

    return 0;
}

void be_jse_timer_cb_handler(void *arg)
{
    jse_task_msg_t *p_jse_msg = (jse_task_msg_t *)jse_osal_get_timer_param(arg);

    jse_osal_lock_mutex(jse_task_mutex, JSE_TASK_MUTEX_WAITIME);
    jse_osal_messageQ_put(jse_task_mq, p_jse_msg, JSE_MSGQ_WAITIME);
    jse_osal_unlock_mutex(jse_task_mutex);
}

void *jse_task_timer_action(uint32_t ms, bone_engine_call_t action, void *arg,
                            jse_timer_type type)
{
    osTimerId timer_id = NULL;
    jse_task_msg_t *p_param =
        (jse_task_msg_t *)jse_calloc(1, sizeof(jse_task_msg_t));

    if (!p_param) return NULL;

    p_param->callback = action;
    p_param->param    = arg;
    p_param->type     = JSE_TASK_MSG_CALLBACK;

    if (type == JSE_TIMER_REPEAT) {
        timer_id = jse_osal_timer_create(be_jse_timer_cb_handler,
                                         TimerRunPeriodic, p_param);
    } else if (type == JSE_TIMER_ONCE) {
        timer_id = jse_osal_timer_create(be_jse_timer_cb_handler, TimerRunOnce,
                                         p_param);
    } else {
        goto fail;
    }

    if (!timer_id) goto fail;

    int32_t ret = jse_osal_timer_start(timer_id, ms);
    if (ret) {
        jse_osal_timer_delete(timer_id);
        goto fail;
    }

    return timer_id;

fail:
    jse_free(p_param);
    return NULL;
}

int32_t jse_task_cancel_timer(void *timerid)
{
    if (!timerid) return -1;

    return jse_osal_timer_delete((osTimerId)timerid);
}

int32_t jse_task_schedule_call(bone_engine_call_t call, void *arg)
{
    jse_task_msg_t msg_buf;
    jse_task_msg_t *p_param = &msg_buf;

    p_param->callback = call;
    p_param->param    = arg;
    p_param->type     = JSE_TASK_MSG_CALLBACK;
    if (jse_task_mq == NULL) {
        jse_warn("jse_task_mq has not been initlized");
        return -1;
    }
    jse_osal_lock_mutex(jse_task_mutex, JSE_TASK_MUTEX_WAITIME);
    jse_osal_messageQ_put(jse_task_mq, p_param, JSE_MSGQ_WAITIME);
    jse_osal_unlock_mutex(jse_task_mutex);
    return 0;
}

int32_t jsengine_task_init()
{
    if (jse_task_mq != NULL) {
        return 0;
    }

    if ((jse_task_mq = jse_osal_messageQ_create(
             JSE_MSGQ_MAX_NUM, sizeof(jse_task_msg_t))) == NULL) {
        jse_error("create messageQ error\r\n");
        return -1;
    }
    if ((jse_task_mutex = jse_osal_new_mutex()) == NULL) {
        jse_error("create mutex error\r\n");
        return -1;
    }
    return 0;
}
