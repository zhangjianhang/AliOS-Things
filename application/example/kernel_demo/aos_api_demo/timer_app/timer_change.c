/*
 * Copyright (C) 2015-2020 Alibaba Group Holding Limited
 */

#include <stdio.h>

#include <aos/kernel.h>
#include <ulog/ulog.h>

/**
 * This use case demonstrates modifying the timer parameters after the timer is created.
 */

/* module name used by ulog */
#define MODULE_NAME "timer_app"

/* timer handle */
static aos_timer_t timer1;

/**
 * timer callback function.
 * The current use case is executed every 1000ms.
 */
static void timer1_func(void *timer, void *arg)
{
    /*
    * Warning: an interface that causes a block is not allowed to be called within this function.
    * Blocking within this function will cause other timers to run incorrectly.
    * The printf function may also block on some platforms, so be careful how you call it.
    */
    printf("[timer_change]timer expires\r\n");
}

void timer_change(void)
{
    int status;

    /**
     * Create timer. Timer starts automatically after successful creation.
     * some of the parameters are as follows:
     * fn:      timer1_func (this function is called when the timer expires)
     * ms:      1000 (the cycle of the timer)
     * repeat:  1 (set to periodic timer)
     */
    status = aos_timer_new(&timer1, timer1_func, NULL, 1000, 1);
    if (status != 0) {
        LOGE(MODULE_NAME, "create timer error");
        return;
    }

    aos_msleep(10000);

    /* stop the timer before modifying the timer parameter */
    aos_timer_stop(&timer1);

    /* the timer cycle is modified to 2000ms */
    aos_timer_change(&timer1, 2000);

    /* start the timer after the timer parameter modification is complete */
    aos_timer_start(&timer1);
}

