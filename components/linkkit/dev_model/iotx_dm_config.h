/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */



#ifndef _IOTX_DM_CONFIG_H_
#define _IOTX_DM_CONFIG_H_

#define IOTX_DM_CLIENT_CONNECT_TIMEOUT_MS     (10000)
#define IOTX_DM_CLIENT_SUB_RETRY_MAX_COUNTS   (3)
#define IOTX_DM_CLIENT_SUB_TIMEOUT_MS         (5000)
#define IOTX_DM_CLIENT_REQUEST_TIMEOUT_MS     (5000)
#define IOTX_DM_CLIENT_KEEPALIVE_INTERVAL_MS  (30000)
#define CONFIG_SERVICE_LIST_MAXLEN            (30)
#define CONFIG_SERVICE_REQUEST_TIMEOUT        (20000)

#ifndef CONFIG_MQTT_TX_MAXLEN
    #define CONFIG_MQTT_TX_MAXLEN           (2048)
#endif

#ifndef CONFIG_MQTT_RX_MAXLEN
    #define CONFIG_MQTT_RX_MAXLEN           (2048)
#endif

#ifndef CONFIG_DISPATCH_QUEUE_MAXLEN
    #define CONFIG_DISPATCH_QUEUE_MAXLEN    (50)
#endif

#ifndef CONFIG_DISPATCH_PACKET_MAXCOUNT
    #define CONFIG_DISPATCH_PACKET_MAXCOUNT (0)
#endif

#ifndef CONFIG_MSGCACHE_QUEUE_MAXLEN
    #define CONFIG_MSGCACHE_QUEUE_MAXLEN    (50)
#endif

#ifndef CONFIG_FOTA_RETRY_INTERNAL_MS
    #define CONFIG_FOTA_RETRY_INTERNAL_MS   (100)
#endif

#endif
