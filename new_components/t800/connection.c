#include <time.h>

#include "connection.h"
#include "esp_log.h"

static const char *TAG = "T800 Connection";

conn_headers_t new_conn_header(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr) {
    conn_headers_t headers = {0};
    headers.iphdr = malloc(sizeof(struct ip_hdr));
    headers.tcphdr = malloc(sizeof(struct tcp_hdr));
    memcpy(headers.iphdr, iphdr, sizeof(struct ip_hdr));
    memcpy(headers.tcphdr, tcphdr, sizeof(struct tcp_hdr));

    return headers;
}


conn_netflow_v5_t new_netflow_v5(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp) {
    conn_netflow_v5_t ntf = {0};
    
    ntf.headers = new_conn_header(iphdr, tcphdr);

    ntf.nexthop = NULL; // we don't know

    ntf.input = NULL;   // we don't care about this
    ntf.output = NULL;  // we don't care about this

    ntf.dPkts = 1;
    ntf.dOctets = IPH_LEN(iphdr);

    ntf.First = time(NULL);
    ntf.Last = ntf.First;

    ntf.src_as = NULL; // we don't care about this
    ntf.dst_as = NULL; // we don't care about this
    
    ntf.src_mask = malloc(sizeof(esp_ip4_addr_t));
    ntf.dst_mask = malloc(sizeof(esp_ip4_addr_t));
    memcpy(ntf.src_mask, netif_ip4_netmask(inp), sizeof(esp_ip4_addr_t));
    memcpy(ntf.dst_mask, netif_ip4_netmask(inp), sizeof(esp_ip4_addr_t));
    
    return ntf;
}

void t800_get_key_from_id(conn_id_t *id, u8_t *key) {
    size_t offset = 0;
    memcpy(key+offset,    &id->ip_src,   sizeof(ip4_addr_p_t));
    offset += sizeof(ip4_addr_p_t);
    memcpy(key+offset,    &id->ip_dst,   sizeof(ip4_addr_p_t));
    offset += sizeof(ip4_addr_p_t);
    memcpy(key+offset,    &id->port_src, sizeof(u16_t));
    offset += sizeof(u16_t);
    memcpy(key+offset,    &id->port_dst, sizeof(u16_t));
}

bool id_cmp(conn_id_t* id1, conn_id_t* id2) {
    return  id1->ip_src.addr == id2->ip_src.addr &&
            id1->ip_dst.addr == id2->ip_dst.addr &&
            id1->port_src == id2->port_src &&
            id1->port_dst == id2->port_dst;
}

bool t800_headers_cmp(conn_headers_t* hdr1, conn_headers_t* hdr2) {
    conn_id_t id1 = {
        .ip_src = hdr1->iphdr->src,
        .ip_dst = hdr1->iphdr->dest,
        .port_src = hdr1->tcphdr->src,
        .port_dst = hdr1->tcphdr->dest,
    };
    conn_id_t id2 = {
        .ip_src = hdr2->iphdr->src,
        .ip_dst = hdr2->iphdr->dest,
        .port_src = hdr2->tcphdr->src,
        .port_dst = hdr2->tcphdr->dest,
    };

    return id_cmp(&id1, &id2);
}
