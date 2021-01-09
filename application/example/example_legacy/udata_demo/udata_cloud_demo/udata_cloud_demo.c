/*
 * Copyright (C) 2015-2019 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "aos/cli.h"
#include "app_entry.h"
#include "aos/kernel.h"
#include "aos/yloop.h"
#include "linkkit/infra/infra_compat.h"
#include "linkkit/infra/infra_defs.h"
#include "linkkit/dev_model_api.h"
#include "netmgr.h"
#include "sensor/sensor.h"
#include "udata/udata.h"
#include "ulog/ulog.h"

#ifndef UDATA_CLOUD_DEMO_NOT_SUPPORT

#ifdef WITH_SAL
#include <atcmd_config_module.h>
#endif

#define PROP_POST_FORMAT_TEMP "{\"CurrentTemperature\":%.1f}"
#define PROP_POST_FORMAT_ACC  "{\"Accelerometer\":{\"x\":%.2f, \"y\":%.2f, \"z\":%.2f}}"

static volatile char g_wifi_connect = 0; /* wifi connect flag */

/* linkkit initialize callback */
static int user_initialized(const int devid)
{
    user_example_ctx_t *user_example_ctx = user_example_get_ctx();

    LOG("Device Initialized, Devid: %d", devid);

    /* Set the linkkit initialize success flag */
    if (user_example_ctx->master_devid == devid) {
        user_example_ctx->master_initialized = 1;
    }

    return 0;
}


/* wifi connect success callback */
static void wifi_service_event(input_event_t *event, void *priv_data)
{
    netmgr_ap_config_t config;

    if (event->type != EV_WIFI) {
        return;
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        return;
    }

    memset(&config, 0, sizeof(netmgr_ap_config_t));
    netmgr_get_ap_config(&config);
    LOG("wifi_service_event config.ssid %s", config.ssid);
    if (strcmp(config.ssid, "adha") == 0 || strcmp(config.ssid, "aha") == 0) {
        return;
    }

#ifdef EN_COMBO_NET
    if (awss_running) {
        awss_success_notify();
    }
#endif

    /* Set wifi connect flag */
    if (g_wifi_connect == 0) {
        g_wifi_connect = 1;
    }
}


/* uData sensor data read and process callback
when sensor data update this function will be called */
void udata_report_demo(sensor_msg_pkg_t *msg)
{
    int         ret = 0;
    uint8_t     param[128];
    udata_pkg_t buf;

    accel_data_t       *acc;
    temperature_data_t *temp;
    user_example_ctx_t *user_ctx = user_example_get_ctx();

    if ((msg == NULL)) {
        return;
    }

    if (msg->cmd == UDATA_MSG_REPORT_PUBLISH) {
        /* Read the sensor data */
        ret = udata_report_publish(msg->value, &buf);
        if (ret != 0) {
            return;
        }

        /* Process the sensor data */
        switch (buf.type) {

             case UDATA_SERVICE_ACC: {
                /* Print the acceleration sensor data */
                acc = (accel_data_t *)(buf.payload);
                printf("\nAcceleration value: x-axis(%.3f g) y-axis(%.3f g) z-axis(%.3f g) \n", ((float)acc->data[0])/1000 , ((float)acc->data[1])/1000, ((float)acc->data[2])/1000);

                if(user_ctx->master_initialized != 0){
                    memset(param, 0, 128);
                    /* build the report payload */
                    sprintf(param, PROP_POST_FORMAT_ACC, ((float)acc->data[0])/1000, ((float)acc->data[1])/1000, ((float)acc->data[2])/1000);

                    /* Report the acceleration data to cloud */
                    ret = IOT_Linkkit_Report(user_ctx->master_devid, ITM_MSG_POST_PROPERTY, (unsigned char *)param, strlen(param) + 1);
                    if(ret == -1){
                        LOG("%s %d\n",  __func__, __LINE__);
                    }
                }

                break;
             }
             case UDATA_SERVICE_TEMP: {
                /* Print the temperature sensor data */
                temp = (temperature_data_t *)(buf.payload);
                printf("\nTemperature value : %.1f centidegree\n", ((float)temp->t)/10);

                if(user_ctx->master_initialized != 0){
                    memset(param, 0, 128);

                    /* build the report payload */
                    sprintf(param, PROP_POST_FORMAT_TEMP, ((float)(temp->t))/10);

                    /* Report the temperature data to cloud */
                    ret = IOT_Linkkit_Report(user_ctx->master_devid, ITM_MSG_POST_PROPERTY, (unsigned char *)param, strlen(param) + 1);
                    if(ret == -1){
                        LOG("%s %d\n", __func__, __LINE__);
                    }
                }

                break;
             }

             default:
                break;
         }

    }
}

/* uData cloud test, include the following functions */
/* 1. wait wifi conect                               */
/* 2. link aliyun server                             */
/* 3. uData start                                    */
/* 4. handle linkkit event                           */
void udata_cloud_test(void* arg)
{
    int ret = 0;

    iotx_linkkit_dev_meta_info_t master_meta_info;

    user_example_ctx_t* user_ctx = user_example_get_ctx();

    (void)arg;

    /* Wait the wifi connect flag to set */
    while(g_wifi_connect == 0){
        aos_msleep(100);
    }

    /* Linkkit start */
    ret = linkkit_init();
    if (ret != 0){
        return;
    }

    /* Register linkkit initialize callback */
    IOT_RegisterCallback(ITE_INITIALIZE_COMPLETED, user_initialized);

    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    memcpy(master_meta_info.product_key, PRODUCT_KEY, strlen(PRODUCT_KEY));
    memcpy(master_meta_info.product_secret, PRODUCT_SECRET, strlen(PRODUCT_SECRET));
    memcpy(master_meta_info.device_name, DEVICE_NAME, strlen(DEVICE_NAME));
    memcpy(master_meta_info.device_secret, DEVICE_SECRET, strlen(DEVICE_SECRET));

    /* Create a new device */
    user_ctx->master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
    if (user_ctx->master_devid < 0) {
        printf("IOT_Linkkit_Open Failed\n");
        return;
    }

    /* Start Connect Aliyun Server */
    ret = IOT_Linkkit_Connect(user_ctx->master_devid);
    if (ret < 0) {
        printf("IOT_Linkkit_Connect Failed\n");
        return;
    }

    /* uData start */
    (void)udata_init();

    /* Register msg handler to receive sensor data */
    ret = udata_register_msg_handler(udata_report_demo);
    if (ret < 0) {
        LOG("%s %d\n", __func__, __LINE__);
        return;
    }

    /* Subscribe acceleration sensor service */
    ret = udata_subscribe(UDATA_SERVICE_ACC);
    if (ret != 0) {
        LOG("%s %d\n", __func__, __LINE__);
    }

    /* Subscribe temperature sensor service */
    ret = udata_subscribe(UDATA_SERVICE_TEMP);
    if (ret != 0) {
        LOG("%s %d\n", __func__, __LINE__);
    }

    /* Enter loop run to handle linkkit event */
    while (1) {
        IOT_Linkkit_Yield(2000);

    }
}


static void start_netmgr(void *p)
{
    LOG("%s\n", __func__);
    aos_msleep(2000);
    netmgr_start(true);
    aos_task_exit(0);
}


int application_start(int argc, char **argv)
{
    int ret;

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

    /* Set debug log show */
    aos_set_log_level(AOS_LL_DEBUG);

    /* Wifi initialize                                         */
    /* User can use the following cli commands to connect wifi */
    /* Clear wifi command:    netmgr clear                     */
    /* Connect wifi command:  netmgr connect ssid passwd       */
    netmgr_init();

    /* Register wifi connect callback */
    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);

    /* Creat task for uData cloud test */
    ret = aos_task_new("udata_cloud_test", udata_cloud_test, NULL, 5120);
    if (ret != 0){
        return -1;
    }

    /* Connect wifi with saved ssid and passwd */
    ret = aos_task_new("netmgr_start", start_netmgr, NULL, 2048);
    if (ret != 0) {
        return -1;
    }

    /* Enter yloop */
    aos_loop_run();

    return 0;
}

#else

int application_start(int argc, char **argv)
{
    printf("udata cloud demo not support\n");

    /*  Enter yloop */
    aos_loop_run();

    return 0;
}

#endif

