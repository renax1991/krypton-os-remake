#include "queue.h"
#include "kmalloc.h"


extern list_head_t tasks_wait;
extern task_t * running_task;

queue_t * create_queue(uint32_t max_slots, uint32_t elem_sz) {
    forbid();
    queue_t * new_queue = (queue_t*) kmalloc(sizeof(queue_t));
    
    if(new_queue == NULL)
        return NULL;
    
    memset(new_queue, 0, sizeof(queue_t));
    
    new_list((list_head_t*) new_queue);
    new_list((list_head_t*) &new_queue->waiters);
    new_queue->free_slots = max_slots;
    new_queue->max_slots = max_slots;
    new_queue->owner = running_task;
    new_queue->elem_sz = elem_sz;
    permit();
    return new_queue;
}

void destroy_queue(queue_t * queue) {
    forbid();
    memset(queue, 0, sizeof(queue_t));
    kfree(queue);
    permit();
}

queue_msg_t * queue_send(queue_t* queue, char * msg, uint32_t mode) {
    task_t * aux;
    queue_msg_t * new_msg;
    
    forbid();
    if (queue->free_slots > 0) {

        new_msg = (queue_msg_t *) kmalloc(sizeof(queue_msg_t));
        memset((uint8_t *) new_msg, 0, sizeof(queue_msg_t));
        
        if (new_msg == NULL)
            return NULL;
            
        if(msg != NULL) {

            new_msg->data_ptr = kmalloc(queue->elem_sz);
        
            if (new_msg->data_ptr == NULL)
                return NULL;
            
            memcpy((uint8_t *) new_msg->data_ptr, (uint8_t *)msg, queue->elem_sz);
        }
        else new_msg->data_ptr = NULL;
        
        add_head(queue, new_msg);
        if(queue->free_slots-- == queue->max_slots) {
            signal(queue->owner, TB_QUEUE);
        }
        permit();
        return new_msg;
    }
    
    if(mode == QM_BLOCKING) {
        do {
            wait(TB_QUEUE, &queue->waiters);
            forbid();
        } while (queue->free_slots == 0);
        new_msg = (queue_msg_t *) kmalloc(sizeof(queue_msg_t));
        memset((uint8_t *)new_msg, 0, sizeof(queue_msg_t));
        if (new_msg == NULL)
            return NULL;
        
        if(msg != NULL) {
            new_msg->data_ptr = kmalloc(queue->elem_sz);
        
            if (new_msg->data_ptr == NULL)
                return NULL;
            
            memcpy((uint8_t *)new_msg->data_ptr, (uint8_t *) msg, queue->elem_sz);
        }
        else new_msg->data_ptr = NULL;
        
        add_head(queue, new_msg);
        queue->free_slots--;
        permit();
        return new_msg;
    }
    else return NULL;
}

char * queue_recv(queue_t * queue, char * ptr, uint32_t mode) {
    queue_msg_t * msg;
    task_t * aux;
    
    forbid();
    if (queue->free_slots < queue->max_slots) {
        msg = remove_tail(queue);
        if(msg->data_ptr != NULL) {
            memcpy((uint8_t *)ptr, (uint8_t *)msg->data_ptr, queue->elem_sz);
            kfree(msg->data_ptr);
        }
        kfree(msg);
        if(queue->free_slots++ == 0) {
            aux = get_head(&queue->waiters);
            while(aux) {
                signal(aux, TB_QUEUE);
                aux = get_next(aux);
            }
        }
        permit();
        return ptr;
    }
    
    if(mode == QM_BLOCKING) {
        do {
            wait(TB_QUEUE, &tasks_wait);
            forbid();
        } while (queue->free_slots == queue->max_slots);
        msg = remove_tail(queue);
        if(msg->data_ptr != NULL) {
            memcpy((uint8_t *)ptr, (uint8_t *)msg->data_ptr, queue->elem_sz);
            kfree(msg->data_ptr);
        }
        kfree(msg);
        queue->free_slots++;
        permit();
        return ptr;
    }
    else return NULL;
}

