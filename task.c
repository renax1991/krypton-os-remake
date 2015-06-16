
#include "task.h"
#include "panic.h"
#include "common.h"
#include "cpu.h"
#include "syscalls.h"

list_head_t tasks_ready;
list_head_t tasks_wait;
task_t * running_task;
int wait_lock;
int forbid_counter;
int k_reenter;
uint32_t sched_state;

void forbid() {
    forbid_counter++;
}

void permit() {
    if(forbid_counter > 0)
        forbid_counter--;
}

task_t * create_task(void* (*code)(void*), char * task_name, uint32_t task_pri, uint32_t stack_size) {
    task_t * new_task = (task_t *) kmalloc(sizeof(task_t));
    uint32_t* new_stack = (uint32_t*) kmalloc(stack_size);
    
    if(new_task == NULL)
        panic("allocating task - not enough memory");
        
    if(new_stack == NULL)
        panic("allocating stack - not enough memory");
        
    memset(new_task, 0, sizeof(task_t));
    
    new_task->task_state.eip = (uint32_t) code;
    new_task->task_state.esp = ((uint32_t) new_stack)+stack_size;
    new_task->task_state.ss = new_task->task_state.ds = 0x23;
    new_task->task_state.cs = 0x1b;
    new_task->task_state.eflags = 0x3200;
    new_task->stack_end = new_stack;
    new_task->ln_link.pri = task_pri;
    new_task->flags |= TS_READY;
    memcpy(new_task->ln_link.name, task_name, MAX_TASK_NAME_LENGTH);
    
    forbid();
    enqueue(&tasks_ready, (list_node_t*) new_task);
    permit();
    return new_task;
}

void destroy_task(task_t * task) {
    forbid();
    remove((list_node_t*) task);
    kfree(task->stack_end);
    kfree(task);
    permit();
    if (task == running_task)
        yield();
}

uint32_t schedule() {
    /* This is the scheduler, called by one of the 2 interrupt handlers. */

    task_t * top_task;

    /* If multitasking is disabled or the kernel was reentered, return immediately */
    if(forbid_counter > 0 || k_reenter > 0)
        return 0;

    sched_state &= (~NEED_SCHEDULE);
    if(running_task->flags & TS_READY) {
        enqueue(&tasks_ready, (list_node_t*) running_task);
    }
    return NEED_TASK_SWITCH;
}

/* This function gets called from the interrupt handler if a task switch
 * is needed by testing NEED_TASK_SWITCH */
void switch_tasks(registers_t * cpu_context) {
    task_t * next_task;

    /* Unset the task's running flag, for the dispatcher to know
       the task is not running, so it will not corrupt it's stack */
    running_task->flags &= (~TS_RUN);
    
    /* Store the task's CPU context into the state structure */
    memcpy(&running_task->task_state, cpu_context, sizeof(registers_t));
    /* We can safely invalidate running_task, because the code above 
       will never be run twice */
    running_task == NULL;
    /* Try to get a task structure from the ready queue */
    while(! (next_task = (task_t*) get_head(& tasks_ready))) {
        /* If we get inside this loop, no task is ready to run,
           so idle the processor until an interrupt comes 
           and readies a task */
        enable();  // Enable interrupts
        asm volatile("hlt"); // Halt the processor

        /* At this time, the processor is halted and k_reenter >= 0.
           Whenever an interrupt fires, k_reenter is incremented when
           the kernel is entered, and decremented when it exits.
           The scheduler can only be called when k_reenter == 0,
           because it means it is the last level before the return to
           ring 3 (k_reenter == -1).
           In other words, tasks that were put in the list will be
           dispatched in priority order */
    }
    /* There is a task to be run - unlink it from the list */
    remove((list_node_t *) next_task);
    /* Set the running task to be the new task */
    running_task = next_task;
    running_task->flags |= TS_RUN;
    /* Restore the task's CPU context */
    memcpy(cpu_context, &running_task->task_state, sizeof(registers_t));
}

uint32_t wait(uint32_t sigs, list_head_t * wait_list)
{
    if (sigs == 0)
        return 0;
    forbid();
    running_task->sigs_waiting |= sigs;
    running_task->sigs_recvd = 0;
    running_task->flags &= ~TS_READY;
    wait_lock++;
    enqueue(wait_list, (list_node_t *) running_task);
    wait_lock--;
    permit();
    yield();
    
    return running_task->sigs_recvd;
}

uint32_t signal(task_t * task, uint32_t sigs)
{
    if((sigs != 0) && (task->sigs_waiting & sigs)) {
        forbid();
        task->flags |= TS_READY;
        task->sigs_waiting &= ~sigs;
        task->sigs_recvd |= sigs;
        remove((list_node_t *) task);
        enqueue(&tasks_ready, (list_node_t *) task);
        permit();
        if(running_task->ln_link.pri <= task->ln_link.pri)
           yield();
        return sigs;
    }
    return 0;
}


void _yield() {
    sched_state |= NEED_SCHEDULE;
    forbid_counter = 0;
}

void yield() {
    system_call(SYSCALL_YIELD, NULL);
}

void delay(uint32_t ticks) {
    forbid();
    running_task->task_delay = ticks;
    permit();
    wait(TB_DELAY, &tasks_wait);
}

