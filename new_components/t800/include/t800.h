#pragma once

#include "stateless.h"
#include "queue.h"
#include "hash.h"
#include "connection.h"

typedef enum _t800_mode {
    UNINITIALIZED,
    STATELESS,
    STATEFULL
} t800_mode;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _t800_config_t {
    err_t (*stateless_eval)(struct ip_hdr *, struct tcp_hdr *);
    err_t (*statefull_eval)(queue_t *);
    t800_mode mode;
} t800_config_t;

void t800_init(t800_config_t cfg);

err_t t800_run(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp);

#ifdef __cplusplus
}
#endif
