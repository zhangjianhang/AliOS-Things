/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "aos/kernel.h"
#include "ulog/ulog.h"
#include "aos/yloop.h"

#include "netmgr.h"
#include "app_entry.h"

#ifdef WITH_SAL
#include <atcmd_config_module.h>
#endif

#ifdef CSP_LINUXHOST
#include <signal.h>
#endif

static char linkkit_started = 0;

static app_main_paras_t entry_paras;

typedef void (*task_fun)(void *);

static void wifi_service_event(input_event_t *event, void *priv_data)
{
    if (event->type != EV_WIFI) {
        return;
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        return;
    }

    netmgr_ap_config_t config;
    memset(&config, 0, sizeof(netmgr_ap_config_t));
    netmgr_get_ap_config(&config);
    LOG("wifi_service_event config.ssid %s", config.ssid);
    if (strcmp(config.ssid, "adha") == 0 || strcmp(config.ssid, "aha") == 0) {
        //clear_wifi_ssid();
        return;
    }

    if (!linkkit_started) {
        aos_task_new("iotx_example",(task_fun)linkkit_main,(void *)&entry_paras,1024*6);
        linkkit_started = 1;
    }
}

#ifdef TEST_LOOP
const  char *input_data[2]= {"mqttapp","loop"};
#endif
int application_start(int argc, char **argv)
{
#ifdef CSP_LINUXHOST
    signal(SIGPIPE, SIG_IGN);
#endif

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

#ifdef TEST_LOOP
    argc = 2;
    argv = (char **)input_data;
#endif    
    entry_paras.argc = argc;
    entry_paras.argv = argv;

    aos_set_log_level(AOS_LL_DEBUG);

    netmgr_init();

    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);

#ifdef AOS_COMP_CLI

#endif
    netmgr_start(false);

    aos_loop_run();

    return 0;
}
