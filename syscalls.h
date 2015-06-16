#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "common.h"
#include "idt.h"

enum SysCalls {
    /* SCHEDULER */
    SYSCALL_YIELD, /* this is needed to force a task switch */
    /* VIRTUAL MEMORY */
    SYSCALL_KMALLOC,
    SYSCALL_KFREE,
    SYSCALL_MMMAP,
    SYSCALL_MMUNMAP,
};

void syscall(registers_t *regs);

void system_call(int, void*);


#endif
