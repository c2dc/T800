#include <time.h>
#include "queue.h"
#include "esp_log.h"

static const char *TAG = "T800 Queue";

queue_t queue_create(data_types type, conn_netflow_v5_t netflow) {
    if (type < 0 || type >= ENUM_LEN) {
        ESP_LOGE(TAG, "invalid queue type");
        abort();
    }

    queue_t q = {
        .type = type,
        .size = -1,
        .pos = 0,
        .netflow = netflow
    };

    return q;
}

void queue_push(queue_t *q, void *element) {
    queue_el_t el;
    if (q->type == U16) {
        el.data.key = (u16_t)element;
    } else if (q->type == HEADERS) {
        el.data.headers = *(conn_headers_t *)element;
        q->netflow.dOctets = IPH_LEN(el.data.headers.iphdr);
        q->netflow.dPkts++;
        q->netflow.Last = time(NULL);
    } 
    
    if (q->size == -1) {
        q->size = (q->size + 1) % MAX_QUARANTINE_SIZE;
        q->element[q->size] = el;
    } else if (queue_is_full(q)) {
        q->size = (q->size + 1) % MAX_QUARANTINE_SIZE;
        q->element[q->size] = el;
        q->pos = (q->pos + 1) % MAX_QUARANTINE_SIZE;
    } else {
        q->size = (q->size + 1) % MAX_QUARANTINE_SIZE;
        q->element[q->size] = el;
    }
}

int queue_is_full(queue_t *q) {
    return (q->pos == q->size + 1) || (q->pos == 0 && q->size == MAX_QUARANTINE_SIZE - 1);
}

queue_el_t queue_front(queue_t *q) {
    return q->element[q->size];
}

