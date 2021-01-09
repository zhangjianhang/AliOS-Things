/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <aos/kernel.h>
#include <aos/yloop.h>
#include <netmgr.h>
#include "ulog/ulog.h"

#ifdef WITH_SAL
#include <atcmd_config_module.h>
#endif

static int lwm2m_client_started = 0;

extern int lwm2m_client(void);

static void wifi_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_WIFI) {
        return;
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        return;
    }

    if(!lwm2m_client_started)
    {
       aos_task_new("lwm2m_client", (void (*)(void *))lwm2m_client, NULL, 1024 * 8);
       lwm2m_client_started = 1;
    }
}

int application_start(int argc, char *argv[])
{
#ifdef WITH_SAL
    sal_device_config_t data = {0};

    data.uart_dev.port = 1;
    data.uart_dev.config.baud_rate = 115200;
    data.uart_dev.config.data_width = DATA_WIDTH_8BIT;
    data.uart_dev.config.parity = NO_PARITY;
    data.uart_dev.config.stop_bits  = STOP_BITS_1;
    data.uart_dev.config.flow_control = FLOW_CONTROL_DISABLED;
    data.uart_dev.config.mode = MODE_TX_RX;

    sal_add_dev(NULL, &data);
    sal_init();
#endif

    /* Set ulog ouput level to INFO */
    aos_set_log_level(AOS_LL_INFO);

    netmgr_init();

    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);

    netmgr_start(false);

    aos_loop_run();

    return 0;
}
