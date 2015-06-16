#ifndef TASK_H
#define TASK_H

#define NEED_SCHEDULE				1
#define TIME_SLICE_EXPIRED			2
#define NEED_TASK_SWITCH			4

#define TB_DELAY					1
#define TB_QUEUE                    2
#define TB_RESUME                   4

#define TS_RUN						1
#define TS_READY					2

#define MAX_TASK_NAME_LENGTH		32

#include "common.h"
#include "idt.h"

struct task_s {
	list_node_t ln_link;
	registers_t task_state;
	uint32_t * stack_end;
	uint32_t flags;
	uint32_t sigs_waiting;
	uint32_t sigs_recvd;
	uint32_t task_delay;
	void *(*atentry)(void *);
	void *(*atexit)(void *);
};

typedef struct task_s task_t;

task_t * create_task(void* (*code)(void*), char * task_name, uint32_t task_pri, uint32_t stack_size);

void destroy_task(task_t * task);

void switch_tasks(registers_t * cpu_context);

uint32_t schedule();

void forbid();

void permit();

void yield();

void _yield();

uint32_t signal(task_t * task, uint32_t sigs);

uint32_t wait(uint32_t sigs, list_head_t * wait_list);

void delay(uint32_t ticks);

#endif
