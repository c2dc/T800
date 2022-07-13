#pragma once

#include "stateless.h"

typedef enum _t800_mode {
    UNINITIALIZED,
    SELECTED,
} t800_mode;

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _t800_config_t {
    err_t (*eval)(struct ip_hdr *, struct tcp_hdr *);
    t800_mode mode;
} t800_config_t;

void t800_init(t800_config_t cfg);

err_t t800_run(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr);

#ifdef __cplusplus
}
#endif
