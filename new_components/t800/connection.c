#include <time.h>

#include "connection.h"
#include "esp_log.h"

static const char *TAG = "T800 Connection";

conn_netflow_v5_t new_netflow_v5(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, struct netif *inp) {
    conn_netflow_v5_t ntf = {0};
    
    ntf.headers.iphdr = *iphdr;
    ntf.headers.tcphdr = *tcphdr;

    ntf.nexthop = NULL; // we don't know

    ntf.input = NULL;   // we don't care about this
    ntf.output = NULL;  // we don't care about this

    ntf.dPkts = 1;
    ntf.dOctets = IPH_LEN(iphdr);

    ntf.First = time(NULL);
    ntf.Last = ntf.First;

    ntf.src_as = NULL; // we don't care about this
    ntf.dst_as = NULL; // we don't care about this
    
    ntf.src_mask = *netif_ip4_netmask(inp);
    ntf.dst_mask = *netif_ip4_netmask(inp);
    
    return ntf;
}

void t800_get_four_tuple(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr, u8_t *key) {
    size_t offset = 0;
    memcpy(key+offset, &iphdr->src,   sizeof(ip4_addr_p_t));
    offset += sizeof(ip4_addr_p_t);
    memcpy(key+offset, &iphdr->dest,  sizeof(ip4_addr_p_t));
    offset += sizeof(ip4_addr_p_t);
    memcpy(key+offset, &tcphdr->src,  sizeof(u16_t));
    offset += sizeof(u16_t);
    memcpy(key+offset, &tcphdr->dest, sizeof(u16_t));
}

bool netflow_cmp(struct ip_hdr *ip1, struct tcp_hdr *tcp1,struct ip_hdr *ip2, struct tcp_hdr *tcp2) {
    return  ip1->src.addr == ip2->src.addr &&
        ip1->dest.addr == ip2->dest.addr &&
        tcp1->src == tcp2->src &&
        tcp1->dest == tcp2->dest;
}


//             --------------- CICFLOW STARTS HERE -------------

void cicflow_update(struct cicflow* cic, struct ip_hdr* iphdr, struct tcp_hdr* tcphdr) {
    // TODO: cic->start

    cic->duration = time(NULL) - cic.start;

    cic->bytes += lwip_ntohs(IPH_LEN(iphdr));
    cic->pkts++;
    cic->pkts_per_sec = cic->pkts / cic->duration;

    time_t cur_time = time(NULL);
    time_t delta = cur_time - dir->last;
    cic->iat_ls += delta;
    cic->iat_ss += (delta * delta);
    cic->iat_min = delta < cic->iat_min ? delta : cic->iat_min;
    cic->iat_max = delta > cic->iat_max ? delta : cic->iat_max;
    cic->iat_mean = cic->iat_ls / (cur_time - cic->start);
    cic->iat_std = sqrt((cic->iat_ss / (cur_time - cic->start)) - (cic->iat_mean * cic->iat_mean));
    cic->last = cur_time;

    int pkt_len = lwip_ntohs(IPH_LEN(iphdr));
    cic->pkt_len_ls += pkt_len;
    cic->pkt_len_min = pkt_len < cic->pkt_len_min ? pkt_len : cic->pkt_len_min;
    cic->pkt_len_max = pkt_len > cic->pkt_len_max ? pkt_len : cic->pkt_len_max;
    cic->pkt_len_mean = cic->pkt_len_ls / cic->total_pkts;
    cic->pkt_len_ss += (pkt_len * pkt_len);
    cic->pkt_len_std = sqrt((cic->pkt_len_ss / cic->pkts) - (cic->pkt_len_mean * cic->pkt_len_mean));
    cic->pkt_len_var = cic->pkt_len_std * cic->pkt_len_std;

    cic->fin_count += TCPH_FLAGS_SET(tcphdr, TCP_FIN);
    cic->syn_count += TCPH_FLAGS_SET(tcphdr, TCP_SYN);
    cic->rst_count += TCPH_FLAGS_SET(tcphdr, TCP_RST);
    cic->psh_count += TCPH_FLAGS_SET(tcphdr, TCP_PSH);
    cic->ack_count += TCPH_FLAGS_SET(tcphdr, TCP_ACK);
    cic->urg_count += TCPH_FLAGS_SET(tcphdr, TCP_URG);
    cic->cwr_count += TCPH_FLAGS_SET(tcphdr, TCP_CWR);
    cic->ece_count += TCPH_FLAGS_SET(tcphdr, TCP_ECE);
    
    if (cic->src.addr == iphdr.src.addr) {
        cic->up += pkt_len;
    } else {
        cic->down += pkt_len;
    }
    
    cic->pkt_size_avg = cic->pkt_len_ls / cic->pkts;
    
    // TODO: pkt_size_avg, act_data_pkt_fwd, seg_size_min
    // TODO: figure out what is active to idle?
    
    if (delta > FLOW_TIMEOUT) { // was idle -> now active 
        cic->idle_min = delta < cic->idle_min ? delta : cic->idle_min; 
        cic->idle_max = delta > cic->idle_max ? delta : cic->idle_max; 
        cic->idle_mean = cic->idle_ls / cic->pkts; 
        cic->pkt_len_ls += delta;
        cic->pkt_len_ss += (delta * delta);
        cic->idle_std = sqrt((cic->idle_ss / cic->pkts) - (cic->idle_mean * cic->idle_mean));
    }
    
}

// 1 - ip4_input - (cic.bwd)
// 2 - ip4_output - (cic.fwd)
void cicflow_update_direction(directed_flow* dir, struct ip_hdr *iphdr, struct tcp_hdr *tcphdr) {
    dir->total_pkts++;

    int pkt_len = lwip_ntohs(IPH_LEN(iphdr));
    dir->total_pkt_len_ls += pkt_len;
    
    dir->pkt_len_min = pkt_len < dir->pkt_len_min ? pkt_len : dir->pkt_len_min;
    dir->pkt_len_max = pkt_len > dir->pkt_len_max ? pkt_len : dir->pkt_len_max;
    dir->pkt_len_mean = dir->total_pkt_len / dir->total_pkts;
    dir->total_pkt_len_ss += (pkt_len * pkt_len);
    dir->pkr_len_std = sqrt((dir->total_pkt_len_ss / dir->total_pkts) - (dir->pkt_len_mean * dir->pkt_len_mean));

    time_t cur_time = time(NULL);
    time_t delta = cur_time - dir->last;
    dir->iat_ls += delta;
    dir->iat_ss += (delta * delta);
    dir->iat_min = delta < dir->iat_min ? delta : dir->iat_min;
    dir->iat_max = delta > dir->iat_max ? delta : dir->iat_max;
    dir->iat_mean = dir->iat_ls / (cur_time - dir->start);
    dir->iat_std = sqrt((dir->iat_ss / (cur_time - dir->start)) - (dir->iat_mean * dir->iat_mean));
    dir->last = cur_time;

    dir->psh_count += TCPH_FLAGS_SET(tcphdr, TCP_PSH);
    dir->urg_count += TCPH_FLAGS_SET(tcphdr, TCP_URG);

    dir->total_hdr_len += IPH_HLEN(iphdr) + TCP_HLEN; 

    dir->pkts_per_sec = dir->total_pkts / (cur_time - dir->start);

    // TODO: bulk;

    // TODO: subflow;
    
    // TODO: initialize init_win;
    
}

