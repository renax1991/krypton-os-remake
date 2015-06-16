#ifndef QUEUE_H
#define QUEUE_H

#include "common.h"
#include "task.h"

#define QM_BLOCKING 1
#define QM_NONBLOCKING 2

struct queue_s {
    list_head_t msg_list;
    uint32_t free_slots;
    uint32_t max_slots;
    uint32_t elem_sz;
    task_t * owner;
    list_head_t waiters;
};

typedef struct queue_s queue_t;

struct queue_msg_s {
    min_node_t mn_link;
    char * data_ptr;
};

typedef struct queue_msg_s queue_msg_t;

queue_t * create_queue(uint32_t max_slots, uint32_t elem_sz);

queue_msg_t * queue_send(queue_t* queue, char * msg, uint32_t mode);

char * queue_recv(queue_t * queue, char * ptr, uint32_t mode);

void destroy_queue(queue_t * queue);

#endif
