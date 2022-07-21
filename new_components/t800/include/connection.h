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
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

void t800_get_four_tuple(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, u8_t *key);
bool netflow_cmp(struct ip_hdr *ip1, struct tcp_hdr *tcp1, struct ip_hdr *ip2, struct tcp_hdr *tcp2);

typedef struct _conn_headers_t {
    struct ip_hdr iphdr;
    struct tcp_hdr tcphdr;
} conn_headers_t;

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

    ip4_addr_t src_mask;
    ip4_addr_t dst_mask;

} conn_netflow_v5_t;

conn_netflow_v5_t new_netflow_v5(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp);

// -------------------------- CICFLOW STARTS HERE ------------------------------
/*
Directional:
total Bwd packets        Total packets in the backward direction
total Length of Bwd Packet  Total size of packet in backward direction
Bwd Packet Length Min    Minimum size of packet in backward direction
Bwd Packet Length Max    Maximum size of packet in backward direction
Bwd Packet Length Mean    Mean size of packet in backward direction
Bwd Packet Length Std    Standard deviation size of packet in backward direction
Bwd IAT Min              Minimum time between two packets sent in the backward direction
Bwd IAT Max              Maximum time between two packets sent in the backward direction
Bwd IAT Mean          Mean time between two packets sent in the backward direction
Bwd IAT Std              Standard deviation time between two packets sent in the backward direction
Bwd IAT Total          Total time between two packets sent in the backward direction
Bwd PSH Flag          Number of times the PSH flag was set in packets travelling in the backward direction (0 for UDP)
Bwd URG Flag          Number of times the URG flag was set in packets travelling in the backward direction (0 for UDP)
Bwd Header Length        Total bytes used for headers in the backward direction
Bwd Packets/s          Number of backward packets per second
AVG Bwd Segment Size     Average number of bytes bulk rate in the backward direction
Bwd Avg Bytes/Bulk        Average number of bytes bulk rate in the backward direction
Bwd AVG Packet/Bulk     Average number of packets bulk rate in the backward direction
Bwd AVG Bulk Rate         Average number of bulk rate in the backward direction
Subflow Bwd Packets        The average number of packets in a sub flow in the backward direction
Subflow Bwd Bytes        The average number of bytes in a sub flow in the backward direction
Init_Win_bytes_backward    The total number of bytes sent in initial window in the backward direction
*/

struct directed_flow {
    size_t total_pkts;
    size_t total_pkt_len_ls;
    size_t total_pkt_len_ss;

    size_t pkt_len_min;  // min, max, mean, std
    size_t pkt_len_max;  // min, max, mean, std
    size_t pkt_len_mean; // min, max, mean, std
    size_t pkt_len_std; // min, max, mean, std
    
    time_t iat_min; // min, max, mean, std
    time_t iat_max; // min, max, mean, std
    time_t iat_mean; // min, max, mean, std
    time_t iat_std; // min, max, mean, std
    time_t iat_ls; // min, max, mean, std
    time_t iat_ss; // min, max, mean, std

    size_t psh_count;
    size_t urg_count;

    size_t total_hdr_len;

    time_t start;
    time_t last;
    float pkts_per_sec;  // we need a time_t start for division

    float seg_size;  // avg
    float bytes_bulk;  // avg
    float packet_bulk;  // avg
    float bulk_rate;  // avg

    float subflow_pkts;
    float subflow_bytes;

    size_t init_win; // upon creation only, never updated!
};

/* 
General:
Flow duration      Duration of the flow in Microsecond
Flow Byte/s      Number of flow bytes per second
Flow Packets/s      Number of flow packets per second 
Flow IAT Mean      Mean time between two packets sent in the flow
Flow IAT Std      Standard deviation time between two packets sent in the flow
Flow IAT Max      Maximum time between two packets sent in the flow
Flow IAT Min      Minimum time between two packets sent in the flow
Min Packet Length     Minimum length of a packet
Max Packet Length     Maximum length of a packet
Packet Length Mean     Mean length of a packet
Packet Length Std    Standard deviation length of a packet
Packet Length Variance    Variance length of a packet
FIN Flag Count       Number of packets with FIN
SYN Flag Count       Number of packets with SYN
RST Flag Count       Number of packets with RST
PSH Flag Count       Number of packets with PUSH
ACK Flag Count       Number of packets with ACK
URG Flag Count       Number of packets with URG
CWR Flag Count       Number of packets with CWE
ECE Flag Count       Number of packets with ECE
down/Up Ratio      Download and upload ratio
Average Packet Size     Average size of packet
Act_data_pkt_forward    Count of packets with at least 1 byte of TCP data payload in the forward direction
min_seg_size_forward    Minimum segment size observed in the forward direction
Active Min      Minimum time a flow was active before becoming idle
Active Mean      Mean time a flow was active before becoming idle
Active Max      Maximum time a flow was active before becoming idle
Active Std      Standard deviation time a flow was active before becoming idle
Idle Min      Minimum time a flow was idle before becoming active
Idle Mean      Mean time a flow was idle before becoming active
Idle Max      Maximum time a flow was idle before becoming active
Idle Std      Standard deviation time a flow was idle before becoming active
*/

#define FLOW_TIMEOUT 15

struct cicflow {
    time_t start;
    time_t last;
    size_t duration;

    size_t bytes; // bytes/sec
    size_t pkts; // packets/sec
    float pkts_per_sec;

    time_t iat_min; // min, max, mean, std
    time_t iat_max; // min, max, mean, std
    time_t iat_mean; // min, max, mean, std
    time_t iat_std; // min, max, mean, std
    time_t iat_ls; // min, max, mean, std
    time_t iat_ss; // min, max, mean, std
                   
    size_t pkt_len_min; // min, max, mean, std, var
    size_t pkt_len_max; // min, max, mean, std, var
    size_t pkt_len_mean; // min, max, mean, std, var
    size_t pkt_len_std; // min, max, mean, std, var
    size_t pkt_len_ls; // min, max, mean, std, var
    size_t pkt_len_ss; // min, max, mean, std, var
    size_t pkt_len_var;

    size_t fin_count;
    size_t syn_count;
    size_t rst_count;
    size_t psh_count;
    size_t ack_count;
    size_t urg_count;
    size_t cwr_count;
    size_t ece_count;
    
    ip4_addr_t src;
    size_t down;    // this
    size_t up;      // + this -> down/up ratio

    size_t pkt_size_avg; // avg
    size_t act_data_pkt_fwd;
    size_t seg_size_min;

    size_t active_min; // min, max, mean, std
    size_t active_max; // min, max, mean, std
    size_t active_mean; // min, max, mean, std
    size_t active_stf; // min, max, mean, std

    size_t idle_min; // min, max, mean, std
    size_t idle_max; // min, max, mean, std
    size_t idle_mean; // min, max, mean, std
    size_t idle_std; // min, max, mean, std
    size_t idle_ls; // min, max, mean, std
    size_t idle_ss; // min, max, mean, std
    
    struct directed_flow fwd;
    struct directed_flow bwd;
};

#ifdef __cplusplus
}
#endif

#endif
