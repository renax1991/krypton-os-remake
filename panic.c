//
// panic.c -- Defines the interface for bringing the system to an abnormal halt.
//            Written for JamesM's kernel development tutorials.
//      Rewritten for Krypton kernel

#include "panic.h"
#include "common.h"
#include "kprintf.h"
#include "idt.h"

static void keyboard_reset_on_panic(registers_t * regs);


void panic (const char *msg)
{
  kprintf ("\n-----------------------\nSorry, a system error ocurred: %s\n", msg);
  asm volatile("cli");
  asm volatile("hlt");
}

static void keyboard_reset_on_panic(registers_t * regs)
{
  asm volatile ("mov $0, %eax; \
                 mov %eax, %cr3;");
  /* Hopefully this will not be reached */
}
