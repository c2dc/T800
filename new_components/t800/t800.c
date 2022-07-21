#include "t800.h"
#include "esp_log.h"

static const char *TAG = "T800";

// ======== Firewall structures ========
typedef struct _flow {
    conn_netflow_v5_t netflow;
    bool is_malicious;
} flow_t;

#define MAX_CONNECTIONS 10
typedef struct _flow_hash {
    flow_t flows[MAX_CONNECTIONS];
    size_t size;
} flow_hash_t;

flow_hash_t flow_hash;
t800_config_t config;
// =====================================

bool hash_is_full() {
    return (flow_hash.size == MAX_CONNECTIONS - 1);
}

// In case flow is blacklisted, return true
flow_t *flow_hash_get(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr){
    // 4 + 4 + 2 + 2 = 4-tuple size = 12 bytes
    u8_t key[12] = {0};
    t800_get_four_tuple(iphdr, tcphdr, key);

    unsigned hash_key = wyhash32(key, 12, 0xcafebabe);
    
    u16_t idx = hash_key % MAX_CONNECTIONS;

    int visited = 0;    // Since linear probing is used, key may not
    while (visited++ < MAX_CONNECTIONS) { // map directly to its position
        flow_t *wanted_flow = &flow_hash.flows[idx];
        if (netflow_cmp(iphdr, tcphdr, &wanted_flow->netflow.headers.iphdr, &wanted_flow->netflow.headers.tcphdr)) {
            return wanted_flow;
        }

        idx = (idx + 1) % MAX_CONNECTIONS;
    }

    return NULL;
}

// Insert in blacklist - if a collision happens, insert anyway
flow_t *flow_hash_insert(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp){
    u8_t key[12] = {0};
    t800_get_four_tuple(iphdr, tcphdr, key);

    unsigned hash_key = wyhash32(key, 12, 0xcafebabe);

    conn_netflow_v5_t netflow = new_netflow_v5(iphdr, tcphdr, inp);
    flow_t new_flow = {
        .netflow = netflow, 
        .is_malicious = false
    };

    u16_t idx = hash_key % MAX_CONNECTIONS;
    if (!hash_is_full()) {
        int visited = 0;    // since linear probing is used, key may not
        while (visited++ < MAX_CONNECTIONS) { // map directly to its position
            flow_t *wanted_flow = &flow_hash.flows[idx];
            if (wanted_flow->netflow.headers.tcphdr.src == 0) {
                break;
            }

            idx = (idx + 1) % MAX_CONNECTIONS;
        }

        flow_hash.flows[idx] = new_flow;
        flow_hash.size++;
    } else {
        flow_hash.flows[idx] = new_flow;
    }

    return &flow_hash.flows[idx];
}

// TODO: vai dar merda o netif aqui!
err_t t800_statefull(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp) {
    flow_t *flow = flow_hash_get(iphdr, tcphdr);

    if (flow) {  // packet already has a flow
        // TODO: update cicflow here
        flow->netflow.dOctets = IPH_LEN(iphdr);
        flow->netflow.dPkts++;
        flow->netflow.Last = time(NULL);

        printf("flow found!\n");
    } else {  // packet introduces new flow
        flow = flow_hash_insert(iphdr, tcphdr, inp);
    }

    if (flow->is_malicious) {
        /* esp_loge(tag, "flow in blacklist"); */
        return ERR_ABRT;
    } else if (config.statefull_eval(&flow->netflow) == ERR_ABRT) { 
        /* esp_loge(tag, "flow malicious"); */
        flow->is_malicious = true;
        return ERR_ABRT;
    }

    return ERR_OK;
}

void t800_init(t800_config_t cfg) {
    config = cfg;
    flow_hash.size = 0;
}

err_t t800_run(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp) {
    if (IPH_PROTO(iphdr) != IP_PROTO_TCP) {
        /* ESP_LOGI(TAG, "IS_NOT_A_TCP_PACKET"); */
        return ERR_OK;
    }

    switch (config.mode) {
        case UNINITIALIZED:
            return ERR_OK;
        case STATELESS:
            return config.stateless_eval(iphdr, tcphdr);
        case STATEFULL:
            return t800_statefull(iphdr, tcphdr, inp);
    }

    return ERR_OK;
}
