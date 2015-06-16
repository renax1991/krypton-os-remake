//
// timer.c -- Initialises the PIT, and handles clock updates.
//            Written for JamesM's kernel development tutorials.
//          Rewritten for the Krypton kernel

#include "common.h"
#include "timer.h"
#include "idt.h"
#include "task.h"

extern uint32_t sched_state;
extern list_head_t tasks_wait;
extern wait_lock;

uint32_t system_tick = 0;

static void timer_callback (registers_t *regs)
{
    task_t * tasks_waiting;
    system_tick++;
    
    tasks_waiting = get_head(&tasks_wait);
    if (wait_lock == 0)
     while(tasks_waiting) {
        if(tasks_waiting->sigs_waiting & TB_DELAY)
            if(--(tasks_waiting->task_delay) <= 0) {
                asm volatile("cli");
                signal(tasks_waiting, TB_DELAY);
                asm volatile("sti");
            }
        tasks_waiting = get_next(tasks_waiting);
    }
    
    sched_state |= NEED_SCHEDULE;
}

void init_timer (uint32_t frequency)
{
  // Firstly, register our timer callback.
  register_interrupt_handler(IRQ0, &timer_callback);

  // The value we send to the PIT is the value to divide it's input clock
  // (1193180 Hz) by, to get our required frequency. Important to note is
  // that the divisor must be small enough to fit into 16-bits.
  uint32_t divisor = 1193180 / frequency;

  // Send the command byte.
  outb(0x43, 0x36);

  // Divisor has to be sent byte-wise, so split here into upper/lower bytes.
  uint8_t l = (uint8_t)(divisor & 0xFF);
  uint8_t h = (uint8_t)( (divisor>>8) & 0xFF );

  // Send the frequency divisor.
  outb(0x40, l);
  outb(0x40, h);
}
