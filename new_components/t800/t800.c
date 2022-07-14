#include "t800.h"
#include "esp_log.h"

static const char *TAG = "T800";

// ======== Firewall structures ========
typedef struct _flow {
    conn_id_t id;
    queue_t headers;
    bool is_malicious;
} flow_t;

typedef struct _flow_hash {
    flow_t flows[MAX_CONNECTIONS];
    queue_t history;
} flow_hash_t;

flow_hash_t flow_hash;
t800_config_t config;
// =====================================

// In case flow is blacklisted, return true
flow_t *flow_hash_get(flow_hash_t* hash, conn_id_t* id){
    // 4 + 4 + 2 + 2 = 4-tuple size = 12 bytes
    u8_t key[12] = {0};
    t800_get_key_from_id(id, key);
    unsigned hash_key = wyhash32(key, 12, 0xcafebabe);
    u16_t idx = hash_key % MAX_CONNECTIONS;

    int visited = 0;    // Since linear probing is used, key may not
    while (visited++ < MAX_CONNECTIONS) { // map directly to its position
        flow_t *wanted_flow = &hash->flows[idx];
        if (id_cmp(id, &wanted_flow->id)) {
            return wanted_flow;
        }

        idx = (idx + 1) % MAX_CONNECTIONS;
    }

    return NULL;
}

// Insert in blacklist - if a collision happens, insert anyway
flow_t *flow_hash_insert(flow_t *flow, flow_hash_t *hash){
    u8_t key[12] = {0};
    t800_get_key_from_id(&flow->id, key);
    unsigned hash_key = wyhash32(key, 12, 0xcafebabe);
    u16_t idx = hash_key % MAX_CONNECTIONS;
    
    if (!queue_is_full(&hash->history)) {
        int visited = 0;    // Since linear probing is used, key may not
        while (visited++ < MAX_CONNECTIONS) { // map directly to its position
            flow_t *wanted_flow = &hash->flows[idx];
            if (wanted_flow->id.port_src == 0) {
                break;
            }

            idx = (idx + 1) % MAX_CONNECTIONS;
        }

        hash->flows[idx] = *flow;
        queue_push(&hash->history, (void *)idx);
        
        return &hash->flows[idx];
    } else {
        flow_t *collision_flow = &hash->flows[idx]; 

        // Get stale flow and swap it with flow from collision
        u16_t oldest_idx = queue_front(&hash->history).data.key;
        flow_t *oldest_flow = &hash->flows[oldest_idx];
        
        int limit = queue_is_full(&oldest_flow->headers) ? MAX_QUARANTINE_SIZE : 
            oldest_flow->headers.size;
        for (int i = 0; i < limit; i++) {
            free(oldest_flow->headers.element[i].data.headers.iphdr);
            free(oldest_flow->headers.element[i].data.headers.tcphdr);
        }

        // swap oldest with the flow from collision
        *oldest_flow = *collision_flow;
        *collision_flow = *flow;

        queue_push(&hash->history, (void *)idx);
        return collision_flow;
    }
}

err_t t800_statefull(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp) {
    conn_id_t id = {
        .ip_src   = iphdr->src,
        .ip_dst   = iphdr->dest,
        .port_src = tcphdr->src,
        .port_dst = tcphdr->dest
    };
    conn_headers_t headers = new_conn_header(iphdr, tcphdr);

    flow_t *flow = flow_hash_get(&flow_hash, &id);
    if (flow != NULL) {  // Packet already has a flow
        // Insert current packet headers
        if (queue_is_full(&flow->headers)) {
            conn_headers_t old_headers = queue_front(&flow->headers).data.headers;
            free(old_headers.iphdr);
            free(old_headers.tcphdr);
        }
    } else {  // Packet introduces new flow
        conn_netflow_v5_t netflow = new_netflow_v5(iphdr, tcphdr, inp);
        flow_t new_flow = {
            .id = id, 
            .headers = queue_create(HEADERS, netflow), 
            .is_malicious = false
        };

        flow = flow_hash_insert(&new_flow, &flow_hash);
    }
    queue_push(&flow->headers, &headers);

    if (queue_is_full(&flow->headers)) {
        if (flow->is_malicious) {
            /* ESP_LOGE(TAG, "flow in blacklist"); */
            return ERR_ABRT;
        } else if (config.statefull_eval(&flow->headers) == ERR_ABRT) {
            /* ESP_LOGE(TAG, "flow malicious"); */
            flow->is_malicious = true;
            return ERR_ABRT;
        }
    } 

    return ERR_OK;
}

void t800_init(t800_config_t cfg) {
    config = cfg;
    flow_hash.history.type = U16;
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
