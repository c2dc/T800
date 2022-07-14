#ifndef _CONN_ID_H_
#define _CONN_ID_H_

#include "lwip/opt.h"

#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/ip4_addr.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/prot/ip4.h"
#include "lwip/prot/tcp.h"
#include "esp_netif.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _conn_id_t {
  ip4_addr_p_t ip_src;
  ip4_addr_p_t ip_dst;
  u16_t port_src;
  u16_t port_dst;
} conn_id_t;

void t800_get_key_from_id(conn_id_t *id, u8_t *key);
bool id_cmp(conn_id_t* id1, conn_id_t* id2);

typedef struct _conn_headers_t {
  struct ip_hdr *iphdr;
  struct tcp_hdr *tcphdr;
} conn_headers_t;

conn_headers_t new_conn_header(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr);

// Compare both the ip_header checksum and the tcp_header checksum of both headerss
bool t800_headers_cmp(conn_headers_t* hdr1, conn_headers_t* hdr2);

typedef struct _conn_netflow_v5_t {
    conn_headers_t headers;

    ip4_addr_p_t *nexthop;

    void *input;   // we don't care about this
    void *output;  // we don't care about this

    u32_t dPkts;
    u32_t dOctets;

    time_t First;
    time_t Last;

    void *src_as; // we don't care about this
    void *dst_as; // we don't care about this

    esp_ip4_addr_t *src_mask;
    esp_ip4_addr_t *dst_mask;

} conn_netflow_v5_t;

conn_netflow_v5_t new_netflow_v5(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp);

#ifdef __cplusplus
}
#endif

#endif
