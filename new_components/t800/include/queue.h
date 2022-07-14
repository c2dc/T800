 #pragma once

#include "connection.h"

#define MAX_CONNECTIONS 11
#define MAX_QUARANTINE_SIZE 4

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    U16,
    HEADERS,
    ENUM_LEN 
} data_types;

typedef struct _queue_el {
    union {
        u16_t key;
        conn_headers_t headers;
    } data;
} queue_el_t;

typedef struct _queue {    
    queue_el_t element[MAX_QUARANTINE_SIZE];
    data_types type;
    int pos;
    int size;
    conn_netflow_v5_t netflow;
} queue_t;

queue_t queue_create(data_types type, conn_netflow_v5_t netflow);
void queue_push(queue_t *q, void *element);
int queue_is_full(queue_t *q);
queue_el_t queue_front(queue_t *q);

#ifdef __cplusplus
}

#endif
