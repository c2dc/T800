#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lwip/ip.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/tcp.h"

err_t decision_tree_depth_10(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr);
err_t decision_tree_depth_12(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr);

err_t mlp(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr);

#ifdef __cplusplus
}
#endif
