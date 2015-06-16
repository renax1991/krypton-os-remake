#include "device.h"

list_head_t device_list;

void register_device_node(device_t * dev) {
    forbid();
    add_head(&device_list, dev);
    permit();
}

device_t * get_device(char * dev_name) {
    device_t * aux;
    
    forbid();
    aux = get_head(&device_list);
    while(aux) {
        if(strcmp(dev_name, aux->ln_link.name) == 0)
            break;
        else
            aux = get_next(aux);
    }
    permit();
    
    return aux;
}

iorq_t * do_io(device_t * dev, iorq_t * req) {
    queue_t * recv_queue;
    
    recv_queue = create_queue(1, sizeof(iorq_t));
    
    if(recv_queue == NULL) {
        return NULL;
    }
        
    req->io_response = recv_queue;
    queue_send(dev->iorq_queue, req, QM_BLOCKING);
    queue_recv(recv_queue, NULL, QM_BLOCKING);
    destroy_queue(recv_queue);
    return req;
}

uint32_t send_io(device_t * dev, iorq_t * req) {
    if(req->io_response == NULL)
        return NULL;
    return queue_send(dev->iorq_queue, req, QM_NONBLOCKING);   
}
