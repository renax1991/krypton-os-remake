#include "syscalls.h"
#include "task.h"
#include "kmalloc.h"
#include "mm.h"

extern uint32_t sched_state;
extern uint32_t forbid_counter;

typedef struct {
        void * physaddr;
        void * virtualaddr;
        unsigned int flags;
} mm_args;


void syscall(registers_t *regs) {
    uint32_t * ret_val = (uint32_t *) regs->ebx;
    mm_args * mm = (mm_args *) regs->ebx;
    
    asm volatile("sti");
    switch(regs->eax) {
        case SYSCALL_YIELD: _yield(); break;
        case SYSCALL_KMALLOC: *ret_val = _kmalloc(*ret_val); break;
        case SYSCALL_KFREE: _kfree(regs->ebx); break;
        case SYSCALL_MMMAP: mm->virtualaddr = mm_map(mm->physaddr, mm->virtualaddr, mm->flags); break;
        case SYSCALL_MMUNMAP: mm_unmap(regs->ebx); break;
    }
    asm volatile("cli");
}

void system_call(int call_no, void * arg) {

    asm volatile("mov %0, %%eax; \
                  mov %1, %%ebx; \
                  int $0xFF;" :: "r" (call_no), "r" (arg) : "%eax", "%ebx");
                  
}

