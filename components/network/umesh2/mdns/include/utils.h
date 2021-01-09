#ifndef __MDNS_UTILS_H__
#define __MDNS_UTILS_H__

#include <inttypes.h>
#include <stdio.h>
#include <stdint.h>
#include <ulog/ulog.h>
#include <aos/kernel.h>
#include <network/network.h>

#define MDNS_DN_MAXSZ 256 // domain name maximum size


#define LOG_TAG "MDNS"
#define log_d(...)          ulog(LOG_DEBUG, LOG_TAG, ULOG_TAG, __VA_ARGS__)
#define log_i(...)          ulog(LOG_INFO, LOG_TAG, ULOG_TAG, __VA_ARGS__)
#define log_w(...)          ulog(LOG_WARNING, LOG_TAG, ULOG_TAG, __VA_ARGS__)
#define log_e(...)          ulog(LOG_ERR, LOG_TAG, ULOG_TAG, __VA_ARGS__)

#define hal_printf          printf
#define hal_malloc          aos_malloc
#define hal_free            aos_free

#define MDNS_POLL_TIMEOUT_S   1

# define INVALID_SOCKET     -1

enum {
    MDNS_STDERR = -1, // standard error
    MDNS_NETERR = -2, // network error
    MDNS_LKPERR = -3, // lookup error
    MDNS_ERROR  = -4, // any runtime error that's not originating from the standard library
};

static inline int ss_family(const struct sockaddr_storage *ss)
{
    return (((const struct sockaddr *) ss)->sa_family);
}

static inline int ss_level(const struct sockaddr_storage *ss)
{
    return (ss_family(ss) == AF_INET ? IPPROTO_IP : IPPROTO_IPV6);
}

static inline socklen_t ss_len(const struct sockaddr_storage *ss)
{
    return (ss_family(ss) == AF_INET ? sizeof(struct sockaddr_in)
            : sizeof(struct sockaddr_in6));
}

static inline uint8_t *write_u16(uint8_t *p, uint16_t *left_len, const uint16_t v)
{
    if (left_len != NULL) {
        if (*left_len < 2) {
            return NULL;
        }
        left_len -= 2;
    }
    *p++ = (v >> 8) & 0xFF;
    *p++ = (v >> 0) & 0xFF;
    return (p);
}

static inline uint8_t *write_u32(uint8_t *p, uint16_t *left_len, const uint32_t v)
{
    if (left_len != NULL) {
        if (*left_len < 4) {
            return NULL;
        }
        left_len -= 4;
    }
    *p++ = (v >> 24) & 0xFF;
    *p++ = (v >> 16) & 0xFF;
    *p++ = (v >>  8) & 0xFF;
    *p++ = (v >>  0) & 0xFF;
    return (p);
}

static inline uint8_t *write_raw(uint8_t *p, uint16_t *left_len, const uint8_t *v)
{
    uint32_t len;

    len = strlen((const char *) v) + 1;
    if (left_len != NULL) {
        if (*left_len < len) {
            return NULL;
        }
        left_len -= len;
    }
    memcpy(p, v, len);
    p += len;
    return (p);
}

static inline const uint8_t *read_u16(const uint8_t *p, uint32_t *s, uint16_t *v)
{
    if (*s < 2 || p == NULL) {
        return NULL;
    }
    *v = 0;
    *v |= (uint16_t) * p++ << 8;
    *v |= (uint16_t) * p++ << 0;
    *s -= 2;
    return (p);
}

static inline const uint8_t *read_u32(const uint8_t *p, uint32_t *s, uint32_t *v)
{
    if (*s < 4 || p == NULL) {
        return NULL;
    }
    *v = 0;
    *v |= (uint32_t) * p++ << 24;
    *v |= (uint32_t) * p++ << 16;
    *v |= (uint32_t) * p++ << 8;
    *v |= (uint32_t) * p++ << 0;
    *s -= 4;
    return (p);
}

#endif /* __MDNS_UTILS_H__ */
