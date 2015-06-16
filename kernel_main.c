
#include "cpu.h"
#include "pmm.h"
#include "common.h"
#include "monitor.h"
#include "multiboot.h"
#include "kprintf.h"
#include "idt.h"
#include "timer.h"
#include "panic.h"
#include "kmalloc.h"
#include "task.h"
#include "syscalls.h"
#include "console.h"
#include "device.h"


/* Check if the compiler thinks if we are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

#define STD_TS_QUANTUM  5  // standard timeslicing quantum is 5 timer ticks
#define TIMER_FREQUENCY 50
#define KERNEL_STACK_SIZE_WORDS 1024

extern void enter_user_mode();
extern void *kernelpagedirPtr;
extern uint32_t LowRamFreeCount;
extern uint32_t HighRamFreeCount;
extern list_head_t tasks_ready;
extern list_head_t tasks_wait;
extern task_t * running_task;
extern int wait_lock;
extern list_head_t device_list;

char * kernel_task_name = "krypton.library";
uint32_t kernel_stack[KERNEL_STACK_SIZE_WORDS] __attribute__((aligned(4096)));
task_t kernel_task;
extern int forbid_counter;
extern int k_reenter;
extern device_t keybd_device;

char buf[32];
iorq_t con_io;


void * timer_task(void * arg) {
    int hours = 0, mins = 0, seconds = 0;
    static char timebuf[30];
    static char rambuf[30];
    
    monitor_writexy(0, 24, "                                                                                ", 7, 0);
    while(1) {
        sprintf(timebuf, "%d:%02d:%02d", hours, mins, seconds++);
        sprintf(rambuf, "%8d kB RAM free", LowRamFreeCount * 4 + HighRamFreeCount * 4);
        monitor_writexy(70, 24, timebuf, 7, 0);
        monitor_writexy(5, 24, rambuf, 7, 0);
        if (seconds == 60) {
            seconds = 0;
            mins++;
        }
        if (mins == 60) {
            mins = 0;
            hours++;
        }
        delay(50);
    }
}

/* init - Krypton microkernel initialization function 
*/

void init(multiboot_t * mboot_ptr) {
    // Initialize the memory manager and interrupts
    mm_init(mboot_ptr); 
    monitor_init();
    kheap_init();
    register_interrupt_handler(255, &syscall);
    init_timer(50);
    strcpy(kernel_task.ln_link.name, kernel_task_name);
    kernel_task.ln_link.pri = 0;
    new_list(&tasks_ready);
    new_list(&tasks_wait);
    new_list(&device_list);
    set_kernel_stack( ((uint32_t) kernel_stack) + KERNEL_STACK_SIZE_WORDS * sizeof(uint32_t));
    running_task = &kernel_task;
    running_task->flags |= TS_READY | TS_RUN; 
    forbid_counter = 0;
    k_reenter = -1;
    wait_lock = 0;
    create_task(console_device, "org.era.dev.console", 0, 1000);
    create_task(timer_task, "org.era.timetask", 10 , 1000);
    
    
    enter_user_mode();
    
    
    delay(25);
    
    memset(&con_io, 0, sizeof(iorq_t));
    con_io.io_desc = DC_READ;
    con_io.io_sz = 32;
    con_io.io_dptr = &buf[0];
    
    while(1) {
        kprintf("root@localhost:/ # ");
        do_io(&keybd_device, &con_io);
        if(strlen(buf) != 0)
            kprintf("bash: %s: No such file or directory\n", buf);
    }
    kprintf("\nSystem halted.\n");
    wait(TB_RESUME, &tasks_wait);

}


