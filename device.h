#ifndef DEVICE_H
#define DEVICE_H

#include "common.h"
#include "queue.h"
#include "task.h"

enum GEN_DEVICE_COMMANDS {
    DC_OPEN,
    DC_CLOSE,
    DC_WRITE,
    DC_READ,
    DC_FLUSH
};

struct iorq_s {
    queue_t * io_response;
    uint32_t io_desc;
    uint32_t io_sz;
    void * io_dptr;
};

typedef struct iorq_s iorq_t;

struct device_s {
    list_node_t ln_link;
    queue_t * iorq_queue;
};

typedef struct device_s device_t;

/* Register a device queue in the devs list */
void register_device_node(device_t *);

/* Seatch the devices list for a device */
device_t * get_device(char *);

/* Do an syncronous I/O op on a device */
iorq_t * do_io(device_t *, iorq_t *);

/* Do an asyncronous I/O op on a device */
uint32_t send_io(device_t *, iorq_t *);

#endif

