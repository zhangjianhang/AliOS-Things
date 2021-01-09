#include <stdio.h>
#include "define.h"
#include "osal.h"
#include "network/network.h"
#include "aos/yloop.h"
#include "network/umesh2/utils/list.h"
#include "network/umesh2/umesh_api.h"

#define MODE_AUTH         0xf
#define MESH_AUTH_PORT    8791
#define MDNS_PORT         5353

#define MESH_ASK_AUTH_ID       0xfefe0000
#define MESH_RSP_AUTH_ID       0xfefe0001
#define MESH_DELETE_AUTH_ID    0xfefe0002

extern int umesh_wifi_get_mac(uint8_t *mac_str);
extern int umesh_get_ipv6(const uint8_t *mac, uint8_t *ip6_out);

static const uint8_t MCAST_ADDR[16] = {0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xfb};

struct umesh_auth_tlv_fix {
    uint32_t id;
    uint8_t  len;
    uint8_t  flag; /* bit0: pwd_enable, bit1: refuse/accept */
} __attribute__((__packed__));

typedef enum {
    AUTH_CHECK_UNKNOW = -1,
    AUTH_CHECK_NO_MATCH = 0,
    AUTH_CHECK_MATCH,
    AUTH_CHECK_ACCEPT,
    AUTH_CHECK_REFUSE,
    AUTH_CHECK_DELETE,
} auth_check_type_t;
service_state_t *g_service_state = NULL;

typedef void (*sock_read_cb)(int fd, void *arg);
static void  uemsh_sock_read_func(int fd, void *arg);
static auth_check_type_t  umesh_unpack_auth_payload(session_t *session, uint8_t *payload, uint16_t len);
static int umesh_service_free(service_t *service);
static int umesh_create_socket(session_t *session, int mode, sock_read_cb cb)
{
    int fd = -1;
    int ret;
    struct sockaddr_in6 addr;
    uint8_t selfmac[6] = {0};

    service_t *self = session->self;

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    umesh_wifi_get_mac(selfmac);
    umesh_get_ipv6(selfmac, addr.sin6_addr.s6_addr);

    if (mode == MODE_AUTH) {
        addr.sin6_port = htons(MESH_AUTH_PORT);
    } else {
        addr.sin6_port = htons(self->id.port);
    }

    switch (mode) {
        case MODE_AUTH: { /*for auth ,sys used*/
            if (session->fd_auth >= 0) {
                log_d("fd_auth fd already create, fd = %d", session->fd_auth);
                return 0;
            }
            session->fd_auth = lwip_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            if (session->fd_auth < 0) {
                log_e("auth fd create failed,ret = %d", session->fd_auth);
                return UMESH_SRV_ERR_CREATE_SOCKET;
            }
            fd = session->fd_auth;
        }
        break;
        case MODE_RELIABLE: {
            if (session->fd_tcp >= 0) {
                log_d("tcp fd already create, fd = %d", session->fd_tcp);
                return 0;
            }
            session->fd_tcp = lwip_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
            if (session->fd_udp < 0) {
                log_e("tcp fd create failed,ret = %d", session->fd_tcp);
                return UMESH_SRV_ERR_CREATE_SOCKET;
            }
            fd = session->fd_tcp;
        }
        break;
        case MODE_UNRELIABLE: {
            if (session->fd_udp >= 0) {
                log_d("udp fd already create, fd = %d", session->fd_udp);
                return 0;
            }
            session->fd_udp = lwip_socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            if (session->fd_udp < 0) {
                log_e("udp fd create failed,ret = %d", session->fd_tcp);
                return UMESH_SRV_ERR_CREATE_SOCKET;
            }
            fd = session->fd_udp;
        }
        break;
    }
    if (fd < 0) {
        return UMESH_SRV_ERR_CREATE_SOCKET;
    }
    ret = lwip_bind(fd, (const struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        lwip_close(fd);
        log_e("socket(%d) bind failed", fd);
        return UMESH_SRV_ERR_BIND;
    }
    log_d("socket create success");
    if (cb) {
        hal_register_socket_read(fd, cb, session);
    }

    return ret;
}

static int umesh_sendto(int socket, const uint8_t *payload, uint16_t length,
                        struct in6_addr *ip6, uint16_t port)
{

    struct sockaddr_in6 sock_addr;


    sock_addr.sin6_len = sizeof(sock_addr);
    sock_addr.sin6_family = AF_INET6;
    sock_addr.sin6_port = htons(port);
    memcpy(&sock_addr.sin6_addr, ip6, sizeof(struct in6_addr));

    return lwip_sendto(socket, payload, length, 0, (struct sockaddr *)&sock_addr,
                       sizeof(sock_addr));
}

static  int umesh_send_data(session_t *session, struct in6_addr *ip6, uint16_t port, int mode, const uint8_t *data,
                            uint16_t len)
{
    int ret = 0;
    int fd = -1;
    service_t *self = session->self;
    if (self == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }

    ret = umesh_create_socket(session, mode,  uemsh_sock_read_func);
    if (ret < 0) {
        return ret;
    }

    switch (mode) {
        case MODE_RELIABLE:
            fd = session->fd_tcp;
            break;
        case MODE_UNRELIABLE:
            fd = session->fd_udp;
            break;
        case MODE_AUTH:
            fd = session->fd_auth;
            break;
        default:
            break;
    }
    //send data
    ret = umesh_sendto(fd, data, len, ip6, port);
    log_d("umesh_sendto, len = %d ,ret = %d", len, ret);
    return ret;
}

static int service_state_init(service_state_t **state, service_t *self, void *mdns, void *network)
{
    service_state_t *service_state = NULL;
    if (state == NULL || self == NULL || mdns == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    service_state = hal_malloc(sizeof(service_state_t));
    if (service_state == NULL) {
        return UMESH_SRV_ERR_MALLOC_FAILED;
    }
    memset(service_state, 0, sizeof(service_state_t));
    service_state->mdns = mdns;
    service_state->network = network;
    service_state->stop = 1;
    service_state->leave_semp = hal_semaphore_new();
    if (service_state->leave_semp == NULL) {
        log_e("semp create failed!");
        goto err;
    }
    INIT_LIST_HEAD(&service_state->self_service_list);
    INIT_LIST_HEAD(&service_state->found_service_list);

    service_state->lock = hal_mutex_new();
    if (service_state->lock == NULL) {
        goto err;
    }
    hal_mutex_lock(service_state->lock);
    list_add_tail(&self->linked_list, &service_state->self_service_list);
    hal_mutex_unlock(service_state->lock);

    *state = service_state;
    return 0;
err:
    if (service_state->leave_semp) {
        hal_semaphore_free(service_state->leave_semp);
    }

    if (service_state->lock) {
        hal_mutex_free(service_state->lock);
    }

    hal_free(service_state);
    *state = NULL;
    return UMESH_SRV_ERR_INIT;
}


static int stop(void *cbarg)
{
    service_t *node, *next;
    service_state_t *state = (service_state_t *)cbarg;
    if (cbarg == NULL) {
        return 1;
    }
    /*deal srvs timeout here*/
    hal_mutex_lock(state->lock);

    list_for_each_entry_safe(node, next, &state->found_service_list, linked_list, service_t) {
        if (hal_now_ms() - node->last_update > node->ttl * SERVICE_TIMEOUT_CNT * 1000) {
            log_w("%s-%s timeout,now = %ld, last = %ld, ttl = %d, clean it", node->srv_type, node->srv_name, hal_now_ms(),
                  node->last_update, node->ttl);

            if (node->session != NULL) {
                session_t *session = node->session;
                hal_mutex_lock(session->lock);
                list_del(&node->linked_list2);
                node->session = NULL;
                hal_mutex_unlock(session->lock);
                /*session changed*/
                if (session->state_cb) {
                    session->state_cb(session, &node->id, SESSION_MEMBER_LEAVE, session->state_cb_ctx);
                }

            }
            if (state->found_cb) {
                state->found_cb(node, PEER_LOST, state->found_cb_ctx);
            }

            umesh_service_free(node);
        }
    }
    hal_mutex_unlock(state->lock);
    return (int)state->stop;
}

static int send_mdns(void *handle, const char *srv_name, const char *domain, struct mdns_data_txt *txt,
                     struct in6_addr *addr, int port,
                     uint16_t ttl)
{
    struct mdns_ctx *ctx = (struct mdns_ctx *) handle;
    struct mdns_hdr hdr;
    struct mdns_entry answers[4]; // A/AAAA, SRV, TXT, PTR
    char *only_type = NULL;

    memset(&hdr, 0, sizeof(hdr));
    hdr.flags |= FLAG_QR;
    hdr.flags |= FLAG_AA;
    hdr.num_ans_rr = sizeof(answers) / sizeof(answers[0]);
    hdr.num_qn = 0;


    if (handle == NULL || srv_name == NULL || domain == NULL || addr == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    only_type = (char *)strstr(srv_name, ".");
    if (only_type == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    only_type += 1; /*skip '.'*/
    memset(&answers[0], 0, sizeof(struct mdns_entry) * 4);

    for (int i = 0; i < hdr.num_ans_rr; i++) {

        answers[i].class = RR_IN;
        answers[i].ttl      = ttl;

        if (i + 1 < hdr.num_ans_rr) {
            answers[i].next = &answers[i + 1];
        }
    }

    answers[0].type     = RR_PTR;
    answers[0].name     = only_type;
    answers[0].data.PTR.domain = (char *)srv_name;

    answers[1].type     = RR_TXT;
    answers[1].name     = (char *)srv_name;
    answers[1].data.TXT  = txt;

    answers[2].type     = RR_SRV;
    answers[2].name     = (char *)srv_name;
    answers[2].data.SRV.port = port;
    answers[2].data.SRV.priority = 0;
    answers[2].data.SRV.weight = 0;
    answers[2].data.SRV.target = (char *)domain;

    answers[3].name     = (char *)domain;

    answers[3].type     = RR_AAAA;
    memcpy(&answers[3].data.AAAA.addr, addr,
           sizeof(struct in6_addr));
    answers[3].next = NULL;
    return mdns_send(ctx, &hdr, answers);

}

static void umesh_mdns_announce(const char *name, void *context)

{
    service_t *node, *next;

    service_state_t *state = (service_state_t *)context;
    if (state == NULL) {
        return;
    }

    hal_mutex_lock(state->lock);
    if (state->announced == 0) {
        hal_mutex_unlock(state->lock);
        log_w("advertising is canceled, do nothing");
        return;
    }
    list_for_each_entry_safe(node, next, &state->self_service_list, linked_list, service_t) {
        if (strcmp(name, node->srv_type) == 0) {
            char mdns_name[SERVICE_FULL_TYPE_LEN_MAX] = {0};
            char domain[32] = {0};
            snprintf(domain, 31, "umesh2_%02x:%02x:%02x", node->id.ip6.s6_addr[13], node->id.ip6.s6_addr[14],
                     node->id.ip6.s6_addr[15]);
            snprintf(mdns_name, SERVICE_FULL_TYPE_LEN_MAX, "%s.%s.%s", node->srv_name, node->srv_type, SERVICE_TYPE_SUFFIX);

            send_mdns(state->mdns, mdns_name, domain, (struct mdns_data_txt *)node->txt_items,
                      &node->id.ip6, node->id.port, node->ttl);
        }
    }
    hal_mutex_unlock(state->lock);

}

static int get_char_num(const char *str, char a)
{
    int num = 0;
    if (str == NULL) {
        return 0;
    }
    char *p = (char *)str;
    while (*p != 0) {
        if (*p == a) {
            num ++;
        }
        p++;
    }
    return num;
}
static void callback_recv(void *p_cookie, int status, const struct mdns_entry *entries)
{
    int ret = 0;
    struct mdns_entry *entry = NULL;
    service_state_t *state = NULL;
    service_t *service = NULL;

    service_t *node, *next;
    int find = 0;
    uint8_t required = 0;
    if (p_cookie == NULL) {
        return;
    }
    if (status < 0) {
        log_e("get entries err, status = %d", status);
        return;
    }

    state = (service_state_t *)p_cookie;
    service = hal_malloc(sizeof(service_t));
    memset(service, 0, sizeof(service_t));

    if (service == NULL) {
        return ;
    }
    entry = (struct mdns_entry *)entries;
    while (entry != NULL) {
        switch (entry->type) {
            case RR_A:
                break;
            case RR_PTR:
                /*
                                if (entry->data.PTR.domain != NULL) {
                                    strncpy(service->srv_name, entry->data.PTR.domain, SERVICE_NAME_LEN_MAX);
                                }
                */
                break;
            case RR_TXT: {
                txt_item_t *txt_next = entry->data.TXT;
                while (txt_next != NULL) {
                    txt_item_t *txt;
                    txt = hal_malloc(sizeof(txt_item_t));
                    memset(txt, 0, sizeof(txt_item_t));
                    if (txt == NULL) {
                        goto err;
                    }
                    memcpy(txt->txt, txt_next->txt, strlen(txt_next->txt));
                    txt->next = service->txt_items;
                    service->txt_items = txt;
                    txt_next = txt_next->next;
                };
            }
            break;
            case RR_AAAA:
                required |= 0x01;
                memcpy(&service->id.ip6, entry->data.AAAA.addr.s6_addr, sizeof(service->id.ip6));
                break;
            case RR_SRV: {
                char *name = NULL;
                char *type = NULL;
                char *last = NULL;
                int num = get_char_num(entry->name, '.');
                if (num == 2) {
                    name = UMESH_SRV_DEFAULT_NANE;
                    char *type = (char *)strtok_r(entry->name, ".", &last);
                    if (type == NULL || strlen(type) == 0) {
                        break;
                    }
                } else if (num == 3) {
                    name = (char *)strtok_r(entry->name, ".", &last);
                    type = (char *)strtok_r(NULL, ".", &last);
                    if (name == NULL || strlen(name) == 0) {
                        break;
                    }
                    if (type == NULL || strlen(type) == 0) {
                        break;
                    }
                } else {
                    log_w("mdns srv: unknow format!");
                    break;
                }

                required |= 0x02;

                strncpy(service->srv_name, name, SERVICE_NAME_LEN_MAX);
                strncpy(service->srv_type, type, SERVICE_TYPE_LEN_MAX);
                service->ttl = entry->ttl;
                log_d(" srv type = %s,TTL = %d", type, service->ttl);
                if (service->ttl == 0) {
                    service->ttl = SERVICE_TTL;
                }
                service->id.port = entry->data.SRV.port;
            }
            break;
            default:
                break;
        }

        entry = entry->next;
    };

    if ((required & 0x03) != 0x03) {
        log_e("service not complete");
        ret = UMESH_SRV_ERR_SERVICE_INCOMPLETE;
        goto err;
    }
    hal_mutex_lock(state->lock);
    list_for_each_entry_safe(node, next, &state->found_service_list, linked_list, service_t) {
        if (!strcmp(node->srv_name, service->srv_name) && !strcmp(node->srv_type, service->srv_type)) {
            node->last_update = hal_now_ms();
            find = 1;
            break;
        }
    }

    if (!find && list_entry_number(&state->found_service_list) < SERVICE_MAX_FOUND_NUM) {
        log_d("add serivce to found list:%s-%s", service->srv_type, service->srv_name);
        service->last_update = hal_now_ms();
        list_add_tail(&service->linked_list, &state->found_service_list);
        if (state->found_cb) {
            state->found_cb(service, PEER_FOUND, state->found_cb_ctx);
        }
    }  else {
        ret = umesh_service_free(service);
    }
    hal_mutex_unlock(state->lock);
    return;
err:
    log_e("service incomplete, discarded! ret = %d", ret);
    if (service != NULL) {
        umesh_service_free(service);
    }
    return;
}

static void  uemsh_sock_read_func(int fd, void *arg)
{
    service_t *node, *next;
    session_t *session = (session_t *)arg;

    int len = 0;
    int ret;
    int is_auth_data = 0;
    uint8_t *buffer;
    struct sockaddr_in6 addr;
    int addr_len = sizeof(struct sockaddr_in6);

    if (arg == NULL) {
        return;
    }
    bzero(&addr, sizeof(addr));
    addr.sin6_family = AF_INET6;

    addr.sin6_port = 0;
    addr.sin6_addr = in6addr_any;

    buffer =  hal_malloc(SERVICE_DATA_MAX_LEN);
    if (!buffer) {
        return;
    }
    memset(buffer, 0, SERVICE_DATA_MAX_LEN);

    len = recvfrom(fd, buffer, SERVICE_DATA_MAX_LEN, 0, (struct sockaddr *)&addr, (socklen_t *)&addr_len);
    addr.sin6_port = ntohs(addr.sin6_port);
    if (len > 0) {

        log_d("recv data, len = %d , port = %d", len, addr.sin6_port);

        if (fd == session->fd_auth && addr.sin6_port == MESH_AUTH_PORT) {
            is_auth_data = 1;
            log_d("recv auth data!");
        } else {
            peer_id_t id;
            umesh_receive_cb recieve_cb = NULL;
            void *recieve_cb_ctx = NULL;
            hal_mutex_lock(g_service_state->lock);
            hal_mutex_lock(session->lock);
            list_for_each_entry_safe(node, next, &session->peers_list, linked_list2, service_t) {
                if (!memcmp(&addr.sin6_addr, &node->id.ip6, sizeof(node->id.ip6))) { /*data*/
                    if (session->recieve_cb != NULL) {
                        recieve_cb = session->recieve_cb;
                        recieve_cb_ctx = session->recieve_cb_ctx;
                        memcpy(&id, &node->id, sizeof(peer_id_t));
                        break;
                    }
                }
            }
            hal_mutex_unlock(session->lock);
            hal_mutex_unlock(g_service_state->lock);
            if (recieve_cb != NULL) {
                recieve_cb(session, &id, buffer, len, recieve_cb_ctx);
            }
        }

        if (is_auth_data) { /* auth data?*/
            auth_check_type_t type =  umesh_unpack_auth_payload(session, buffer, len);
            log_d("auth type = %d", type);
            switch (type) {
                case AUTH_CHECK_NO_MATCH: {
                    /*send refuse*/
                    struct umesh_auth_tlv_fix tlv;
                    tlv.id = htonl(MESH_RSP_AUTH_ID);
                    tlv.flag = 0x02;
                    log_w("AUTH_CHECK_NO_MATCH ");
                    ret =  umesh_send_data(session, &addr.sin6_addr, MESH_AUTH_PORT, MODE_AUTH, (const uint8_t *)&tlv, sizeof(tlv));
                }
                break;
                case AUTH_CHECK_MATCH:
                    /*send accept*/
                {
                    /*send refuse*/
                    struct umesh_auth_tlv_fix tlv;
                    tlv.id = htonl(MESH_RSP_AUTH_ID);
                    tlv.flag = 0x0;
                    log_w("AUTH_CHECK_MATCH ");
                    if (session->invite_cb) {
                        peer_id_t id;
                        memset(&id, 0, sizeof(id));
                        memcpy(&id.ip6, &addr.sin6_addr, sizeof(id.ip6));
                        ret = session->invite_cb(session, &id, session->invite_cb_ctx);
                        if (ret < 0) {
                            tlv.flag = 0x02;
                            log_w("user refused invite!");
                        }
                    }
                    ret =  umesh_send_data(session, &addr.sin6_addr, MESH_AUTH_PORT, MODE_AUTH, (const uint8_t *)&tlv, sizeof(tlv));
                }
                break;
                case AUTH_CHECK_ACCEPT:
                    /*add peer to session*/
                {
                    int data = type;
                    hal_mutex_lock(g_service_state->lock);

                    list_for_each_entry_safe(node, next, &g_service_state->found_service_list, linked_list, service_t) {
                        if (!memcmp(&node->id.ip6, &addr.sin6_addr, sizeof(addr.sin6_addr))) {
                            service_t *node_ss, *next_ss;
                            int find = 0;
                            hal_mutex_lock(session->lock);

                            list_for_each_entry_safe(node_ss, next_ss, &session->peers_list, linked_list2, service_t) {
                                if (!memcmp(&node_ss->id.ip6, &addr.sin6_addr, sizeof(addr.sin6_addr))) {
                                    log_w("service %s already in session! ignore it", node_ss->srv_name);
                                    find = 1;
                                    break;
                                }
                            }
                            if (find == 1) {
                                hal_mutex_unlock(session->lock);
                                break;
                            }
                            list_add_tail(&node->linked_list2, &session->peers_list);
                            node->session = session;
                            hal_mutex_unlock(session->lock);
                            if (session->state_cb) {
                                session->state_cb(session, &node->id, SESSION_MEMBER_JOIN, session->state_cb_ctx);
                            }
                            ret = hal_queue_send(session->queue, &data, sizeof(data));
                            log_d("found peer in service list");
                            break;
                        }
                    }
                    hal_mutex_unlock(g_service_state->lock);

                }
                log_d("invite accept");
                break;
                case AUTH_CHECK_REFUSE:
                    /*invite failed*/
                {
                    int data = type;
                    log_w("invite refused ");
                    ret = hal_queue_send(session->queue, &data, sizeof(data));
                }

                break;
                case AUTH_CHECK_DELETE:
                    /*invite failed*/
                {
                    log_d("recv sission delete");
                }

                break;

                case AUTH_CHECK_UNKNOW:
                    log_w("auth err unknow");
                    /*no action*/
                    break;
            }
        }

    } else {
        LOG("no data get");
    }
    hal_free(buffer);
}

//////////////
//负责初始化服务，并完成mdns的初始化

service_t *umesh_service_init(net_interface_t interface, const char *srv_name, const char *srv_type, uint16_t port)
{
    service_t *self_srv = NULL;
    struct mdns_ctx *mdns_ctx = NULL;
    uint8_t selfmac[6];
    int ret = 0;

    if (port == MESH_AUTH_PORT) {
        UMESH_SRV_ERR_PORT_OCCUPIED;
    }
    self_srv = hal_malloc(sizeof(service_t));

    if (self_srv == NULL) {
        return NULL;
    }
    memset(self_srv, 0, sizeof(service_t));
    //_get_device_name(device_name);
    strncpy(self_srv->srv_type, srv_type, SERVICE_TYPE_LEN_MAX);
    strncpy(self_srv->srv_name, srv_name, SERVICE_NAME_LEN_MAX);
    self_srv->ttl = SERVICE_TTL;
    self_srv->id.port = port;

    umesh_wifi_get_mac(selfmac);
    umesh_get_ipv6(selfmac, self_srv->id.ip6.s6_addr);

    if (g_service_state != NULL) {
        if (g_service_state->mdns != NULL) {
            mdns_ctx = g_service_state->mdns;
        }
    }

    if (mdns_ctx == NULL) {
        ret = mdns_init(&mdns_ctx, MDNS_ADDR_IPV6, MDNS_PORT);
        if (ret < 0) {
            goto err;
        }
    }

    if (g_service_state == NULL) {
        ret = service_state_init(&g_service_state, self_srv, (void *)mdns_ctx, interface);
        if (ret < 0) {
            goto err;
        }
    } else {
        hal_mutex_lock(g_service_state->lock);
        list_add_tail(&self_srv->linked_list, &g_service_state->self_service_list);
        hal_mutex_unlock(g_service_state->lock);
    }
    return self_srv;
err:
    log_e("umesh_service_init err,ret = %d", ret);
    if (mdns_ctx != NULL) {
        mdns_destroy(mdns_ctx);
    }

    if (self_srv != NULL) {
        hal_free(self_srv);
    }
    return NULL;
}

static int service_state_deinit(service_state_t *state)
{
    service_t *node, *next;
    log_d("service state deinit");
    hal_mutex_lock(state->lock);
    list_for_each_entry_safe(node, next, &state->found_service_list, linked_list, service_t) {
        umesh_service_free(node);
    }

    list_for_each_entry_safe(node, next, &state->self_service_list, linked_list, service_t) {
        umesh_service_free(node);
    }

    hal_mutex_unlock(state->lock);

    hal_semaphore_free(state->leave_semp);
    state->leave_semp = NULL;
    hal_mutex_free(state->lock);
    state->lock = NULL;
    hal_free(state);
    return 0;
}
static int umesh_service_free(service_t *service)
{
    txt_item_t *item;
    if (service == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }

    item = service->txt_items;
    while (item) {
        txt_item_t *temp = item;
        item = item->next;
        hal_free(temp);
    }

    list_del(&service->linked_list);
    list_del(&service->linked_list2);
    hal_free(service);
    return 0;
}

int umesh_service_deinit(service_t *service)
{
    service_t *node, *next;
    int ret = 0;
    if (g_service_state == NULL) {
        return 0;
    }
    hal_mutex_lock(g_service_state->lock);
    list_for_each_entry_safe(node, next, &g_service_state->self_service_list, linked_list, service_t) {
        if (!memcmp(node->srv_name, service->srv_name, strlen(service->srv_name)) &&
            !memcmp(node->srv_type, service->srv_type, strlen(service->srv_type))) { /*data*/
            umesh_service_free(node);
            break;
        }
    }
    hal_mutex_unlock(g_service_state->lock);

    if (!list_empty(&g_service_state->self_service_list)) {
        return 0;
    }
    g_service_state->stop = 1;
    hal_semaphore_wait(g_service_state->leave_semp, (uint32_t) - 1);
    mdns_destroy(g_service_state->mdns);
    g_service_state->mdns = NULL;
    ret =  service_state_deinit(g_service_state);
    if (ret < 0) {
        return ret;
    }

    g_service_state = NULL;
    return ret;
}
int umesh_service_add_txt(service_t *service, const char *txt)
{
    struct mdns_data_txt *ext_txt;

    if (service == NULL || txt == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }

    ext_txt = hal_malloc(sizeof(struct mdns_data_txt));
    if (ext_txt == NULL) {
        return UMESH_SRV_ERR_MALLOC_FAILED;
    }
    strncpy(ext_txt->txt, txt, MDNS_TXT_MAX_LEN);

    ext_txt->next = service->txt_items;
    service->txt_items = ext_txt;
    return 0;
}

static void mdns_main_task(void *para)
{
    int ret = 0;

    service_state_t *state = (service_state_t *) para;
    if (state == NULL) {
        return;
    }

    log_d("mdns task start !");
    ret = mdns_start(state->mdns, NULL, 0, RR_PTR,
                     SERVICE_QUERY_DURATION, MDNS_MATCH_ALL,
                     stop, callback_recv, para);
    if (ret < 0) {
        log_e("mdns start failed!");
        return;
    }
    hal_semaphore_post(state->leave_semp);
    log_i("leave mdns task!");
}

int umesh_start_browse_service(service_t *service, umesh_service_found_cb found, void *user_data)
{
    int ret = 0;

    if (g_service_state == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }

    if (g_service_state->mdns == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }

    hal_mutex_lock(g_service_state->lock);
    g_service_state->found_cb = found;
    g_service_state->found_cb_ctx = user_data;
    hal_mutex_unlock(g_service_state->lock);

    if (g_service_state->stop) {
        g_service_state->stop = 0;
        ret = hal_task_start(mdns_main_task, g_service_state, SERVICE_TASK_STACK_SIZE, HAL_TASK_NORMAL_PRIO);
    }

    return ret;
}

int umesh_stop_browse_service(void)
{
    hal_mutex_lock(g_service_state->lock);
    g_service_state->found_cb = NULL;
    g_service_state->found_cb_ctx = NULL;
    hal_mutex_unlock(g_service_state->lock);
    return 0;
}

int umesh_stop_advertise_service(service_t *service)
{
    if (g_service_state == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }
    if (g_service_state->mdns == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }

    /*check type in umesh_mdns_announce*/
    hal_mutex_lock(g_service_state->lock);
    if (g_service_state->announced) {
        g_service_state->announced = 0;
    }
    hal_mutex_unlock(g_service_state->lock);
    return 0;
}

int umesh_start_advertise_service(service_t *service)
{
    int ret = 0;
    if (g_service_state == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }
    if (g_service_state->mdns == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }

    /*check type in umesh_mdns_announce*/
    hal_mutex_lock(g_service_state->lock);
    if (!g_service_state->announced) {
        g_service_state->announced = 1;
    }
    mdns_announce(g_service_state->mdns, service->srv_type, RR_PTR, umesh_mdns_announce, g_service_state);
    hal_mutex_unlock(g_service_state->lock);

    if (g_service_state->stop) {
        g_service_state->stop = 0;
        ret = hal_task_start(mdns_main_task, g_service_state, SERVICE_TASK_STACK_SIZE, HAL_TASK_NORMAL_PRIO);
    }

    return ret;
}

int umesh_register_state(session_t *session, umesh_session_state_changed_cb cb, void *user_data)
{
    if (session == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    session->state_cb = cb;
    session->state_cb_ctx = user_data;
    return  0;
}

int umesh_register_inviter(session_t *session, umesh_peer_invite_cb cb, void *user_data)
{
    if (session == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    session->invite_cb = cb;
    session->invite_cb_ctx = user_data;
    return 0;
}

int umesh_register_receiver(session_t *session, umesh_receive_cb cb, void *user_data)
{
    if (session == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    session->recieve_cb = cb;
    session->recieve_cb_ctx = user_data;
    return 0;
}
static int umesh_close_socket(session_t *session)
{

    if (session == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }

    session->state_cb = NULL;
    session->recieve_cb = NULL;
    session->invite_cb = NULL;

    if (session->fd_udp >= 0) {
        hal_unregister_socket_read(session->fd_udp, NULL, NULL);
        lwip_close(session->fd_udp);
        session->fd_udp = -1;
    }
    if (session->fd_tcp >= 0) {
        hal_unregister_socket_read(session->fd_tcp, NULL, NULL);
        lwip_close(session->fd_tcp);
        session->fd_tcp = -1;
    }
    if (session->fd_auth >= 0) {
        hal_unregister_socket_read(session->fd_auth, NULL, NULL);
        lwip_close(session->fd_auth);
        session->fd_auth = -1;
    }
    return 0;
}

session_t *umesh_session_init(service_t *service)
{
    session_t *session  = NULL;
    int ret;
    if (service == NULL) {
        return NULL;
    }
    session = hal_malloc(sizeof(session_t));
    if (session == NULL) {
        return NULL;
    }
    memset(session, 0, sizeof(session_t));
    session->fd_udp = -1;
    session->fd_tcp = -1;
    session->fd_auth = -1;
    session->self = service;

    INIT_LIST_HEAD(&session->peers_list);
    // session->state_cb = session_changed_cb;
    // session->invite_cb = invite_cb;
    // session->user_data= cb_data;
    session->lock  = hal_mutex_new();
    if (session->lock == NULL) {
        goto err;
    }
    session->queue_buf = hal_malloc(SERVICE_QUEUE_MSG_SIZE);
    if (session->queue_buf == NULL) {
        goto err;
    }
    session->queue = hal_queue_new(session->queue_buf, SERVICE_QUEUE_MSG_SIZE, SERVICE_QUEUE_MSG_MAX_NUM);
    if (session->queue == NULL) {
        goto err;
    }

    ret = umesh_create_socket(session, MODE_AUTH,  uemsh_sock_read_func);
    if (ret < 0) {
        goto err;
    }

    ret = umesh_create_socket(session, MODE_UNRELIABLE,  uemsh_sock_read_func);
    if (ret < 0) {
        goto err;
    }

    service->session = session;
    return session;
err:
    umesh_close_socket(session);
    if (session->lock) {
        hal_mutex_free(session->lock);
    }
    if (session->queue_buf) {
        hal_free(session->queue_buf);
    }
    if (session->queue) {
        hal_queue_free(session->queue);
    }
    if (session) {
        hal_free(session);
    }
    return NULL;
}
int umesh_session_deinit(session_t *session)
{
    service_t *node, *next;

    if (session == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    if (g_service_state == NULL) {
        return UMESH_SRV_ERR_NOT_INIT;
    }
    hal_mutex_lock(g_service_state->lock);
    hal_mutex_lock(session->lock);
    umesh_close_socket(session);
    list_for_each_entry_safe(node, next, &session->peers_list, linked_list2, service_t) {
        list_del(&node->linked_list2);
        node->session = NULL;
        log_d("delete node...");
    }
    hal_mutex_unlock(session->lock);
    hal_mutex_unlock(g_service_state->lock);
    hal_msleep(2000);

    if (session->lock) {
        hal_mutex_free(session->lock);
        session->lock = NULL;
    }

    if (session->queue_buf) {
        hal_free(session->queue_buf);
        session->queue_buf = NULL;
    }

    if (session->queue) {
        hal_queue_free(session->queue);
        session->queue = NULL;
    }

    if (session) {
        hal_free(session);
    }

    return 0;
}
static int _pack_auth_payload(session_t *session, peer_id_t *dst, uint8_t *payload)
{
    int len = 0;
    struct umesh_auth_tlv_fix tlv_head;
    memset(&tlv_head, 0, sizeof(tlv_head));
    tlv_head.id = htonl(MESH_ASK_AUTH_ID);
    switch (session->auth_type) {
        case  MODE_AUTH_NONE:
            tlv_head.flag = 0;
            tlv_head.len = 0;
            memcpy(payload, &tlv_head, sizeof(tlv_head));
            len = sizeof(tlv_head);
            break;
        case NODE_AUTH_PWD: {
            int offset;
            tlv_head.flag = 1;
            if (session->auth_data == NULL) {
                return UMESH_SRV_ERR_NO_AUTH_DATA;
            }
            tlv_head.len = session->auth_data->real_len;
            memcpy(payload, &tlv_head, sizeof(tlv_head));
            offset = sizeof(tlv_head);
            memcpy(payload + offset, session->auth_data->data, session->auth_data->real_len);
            len = tlv_head.len + offset;
        }

        break;
        default:
            break;
    }
    return len;
}

static auth_check_type_t  umesh_unpack_auth_payload(session_t *session, uint8_t *payload, uint16_t len)
{
    struct umesh_auth_tlv_fix  tlv;
    if (payload == NULL) {
        return AUTH_CHECK_UNKNOW;
    }
    if (len < sizeof(tlv)) {
        return AUTH_CHECK_UNKNOW;
    }
    memcpy(&tlv, payload, sizeof(tlv) < len ? sizeof(tlv) : len);
    tlv.id = ntohl(tlv.id);
    switch (tlv.id) {
        case MESH_ASK_AUTH_ID: {
            if ((tlv.flag & 0x01) == 0) { //no pwd
                if (session->auth_type  != MODE_AUTH_NONE) {
                    return AUTH_CHECK_NO_MATCH;
                } else {
                    return AUTH_CHECK_MATCH;
                }
            } else {
                if (session->auth_type  != MODE_AUTH_NONE) {
                    umesh_auth_data_t *data = session->auth_data;
                    int pwd_len = len - sizeof(struct umesh_auth_tlv_fix);
                    if (pwd_len != data->real_len) {
                        return AUTH_CHECK_NO_MATCH;
                    }

                    if (memcmp(payload + sizeof(struct umesh_auth_tlv_fix), data->data, pwd_len)) {
                        return AUTH_CHECK_NO_MATCH;
                    } else {
                        return AUTH_CHECK_NO_MATCH;
                    }
                } else {
                    return AUTH_CHECK_MATCH;
                }
            }
        }
        break;
        case MESH_RSP_AUTH_ID: {
            log_d("---tlv.flag = %d ---", tlv.flag);
            if ((tlv.flag & 0x02) == 0) {
                log_d("AUTH_CHECK_ACCEPT");
                return AUTH_CHECK_ACCEPT;
            } else {
                log_d("AUTH_CHECK_REFUSE");
                return AUTH_CHECK_REFUSE;
            }
        }
        break;
        case MESH_DELETE_AUTH_ID: {
            log_d("recv session delete");
            return AUTH_CHECK_DELETE;
        }
        break;
        default:
            log_e("recv auth resp err: %d", tlv.id);
            return AUTH_CHECK_UNKNOW;
            break;
    }

    return AUTH_CHECK_UNKNOW;
}

int umesh_session_auth_set(session_t *session, umesh_auth_mode_t mode, uint8_t *pwd, uint16_t pwd_len)
{
    umesh_auth_data_t *auth_data = hal_malloc(sizeof(umesh_auth_data_t));
    if (auth_data == NULL) {
        return UMESH_SRV_ERR_MALLOC_FAILED;
    }
    if (pwd_len > AUTH_DATA_MAX_LEN) {
        return UMESH_SRV_ERR_OUT_OF_BOUNDS;
    }
    switch (mode) {
        case    MODE_AUTH_NONE :
            break;
        case   NODE_AUTH_PWD : {
            memcpy(auth_data->data, pwd, pwd_len);
            auth_data->real_len = pwd_len;
            session->auth_data = auth_data;
            session->auth_type = mode;
        }
        break;
        default:
            break;
    }
    return 0;
}

int umesh_invite_peer(session_t *session, peer_id_t *dst,  int timeout)
{
    int ret = 0;
    int msg = 0;
    uint32_t msg_size = 4;
    uint8_t hello_payload[SERVICE_AUTH_PAYLOAD_MAX];
    if (session == NULL || dst == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    ret = _pack_auth_payload(session, dst, hello_payload);
    if (ret <= 0) {
        return UMESH_SRV_ERR_AUTH;
    }

    //send data
    ret = umesh_send_data(session, &dst->ip6, MESH_AUTH_PORT, MODE_AUTH, hello_payload, ret);

    ret = hal_queue_recv(session->queue, timeout, (void *)&msg, &msg_size);
    log_d("hal_queue_recv ret = %d, msg = %d", ret, ret);
    if (ret == 0) {
        if (msg == AUTH_CHECK_ACCEPT) {

            log_d("invite success! ");
            ret = 0;
        } else {
            log_e("get auth ack err, msg = %d ", msg);
            ret = UMESH_SRV_ERR_AUTH_REFUSE;
        }
    } else {
        ret = UMESH_SRV_ERR_AUTH;
    }
    return ret;
}

int umesh_delete_peer(session_t *session, peer_id_t *dst)
{
    int ret = 0;
    umesh_session_state_changed_cb state_cb = NULL;
    void *state_cb_ctx = NULL;
    struct umesh_auth_tlv_fix tlv;
    service_t *node, *next;
    peer_id_t temp_id;
    if (session == NULL || dst == NULL) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    memset(&tlv, 0, sizeof(tlv));
    tlv.id = htonl(MESH_DELETE_AUTH_ID);
    tlv.flag = 0x0;
    log_d("MESH_DELETE_AUTH_ID ");
    ret =  umesh_send_data(session, &dst->ip6, MESH_AUTH_PORT, MODE_AUTH, (uint8_t *)&tlv, (uint16_t)sizeof(tlv));
    if (ret < 0) {
        return ret;
    }
    hal_mutex_lock(g_service_state->lock);
    hal_mutex_lock(session->lock);
    list_for_each_entry_safe(node, next, &session->peers_list, linked_list2, service_t) {
        if (!memcmp(&dst->ip6, &node->id.ip6, sizeof(node->id.ip6))) { /*data*/
            if (session->state_cb != NULL) {
                state_cb = session->state_cb;
                state_cb_ctx = session->state_cb_ctx;
                memcpy(&temp_id, &node->id, sizeof(temp_id));
            }
            list_del(&node->linked_list2);
            break;
        }
    }
    hal_mutex_unlock(session->lock);
    hal_mutex_unlock(g_service_state->lock);
    if (state_cb) {
        state_cb(session, &temp_id, SESSION_MEMBER_LEAVE, state_cb_ctx);
    }

    return ret;
}

int umesh_send(session_t *session, peer_id_t *dest, const uint8_t *data,  uint16_t len, data_mode_t mode)
{
    if (session == NULL || data == NULL || len <= 0) {
        return UMESH_SRV_ERR_NULL_POINTER;
    }
    if (dest == NULL) {
        struct in6_addr addr;
        service_t *srv = session->self;
        if (srv == NULL) {
            return UMESH_SRV_ERR_NULL_POINTER;
        }
        memset(&addr, 0, sizeof(addr));
        memcpy(addr.s6_addr, MCAST_ADDR, sizeof(MCAST_ADDR));
        return umesh_send_data(session, &addr, srv->id.port, mode, data, len);
    }
    return  umesh_send_data(session, &dest->ip6, dest->port, mode, data, len);
}
