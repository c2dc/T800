#include "t800.h"
#include "esp_log.h"

static const char *TAG = "T800";

// ======== Firewall structures ========
t800_config_t config;
// =====================================

void t800_init(t800_config_t cfg) {
    config = cfg;
}

err_t t800_run(struct ip_hdr *iphdr, struct tcp_hdr *tcphdr) {
    if (IPH_PROTO(iphdr) != IP_PROTO_TCP) {
        /* ESP_LOGI(TAG, "IS_NOT_A_TCP_PACKET"); */
        return ERR_OK;
    }

    switch (config.mode) {
        case UNINITIALIZED:
            return ERR_OK;
        case SELECTED:
            return config.eval(iphdr, tcphdr);
    }

    return ERR_OK;
}
