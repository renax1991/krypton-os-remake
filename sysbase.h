/* 
 * File:   sysbase.h
 * Author: renato
 *
 *  This file declares the system base globals
 *
 *
 * Created on November 15, 2013, 6:27 PM
 */

#ifndef _SYSBASE_H
#define	_SYSBASE_H

#include "pmm.h"
#include "common.h"

#define NEED_SCHEDULE 1
#define TIME_SLICE_EXPIRED 2
#define NEED_TASK_SWITCH 4


extern unsigned int sys_flags;
extern //thread_t * running_thread;
extern uint32_t k_reenter;
extern unsigned int vm_online;
extern unsigned int pm_last_page; // Address of the last never-freed page given
extern unsigned long * mm_free_page_stack_ptr; // Free page stack address
extern unsigned long mm_free_page_stack_max;  // Maximum free page stack address
extern unsigned long free_pages; // Number of free pages
extern heap_header_t * sh_heap; // Public heap address
extern unsigned long sh_heap_max;
extern list_head_t resources_list;
extern list_head_t device_list;
extern list_head_t lib_list;
extern list_head_t msgport_list;
extern list_head_t thread_ready;
extern list_head_t thread_wait;
extern list_head_t intr_list;
extern list_head_t semaphore_list;
extern list_head_t kernel_heap;
extern uint32_t k_heap_end;
extern unsigned long std_ts_quantum;
extern long ts_curr_count;
extern long forbid_counter;
extern long disable_counter;
extern pagedir_t * act_page_directory;


#endif	/* _SYSBASE_H */

