#include "console.h"
#include "idt.h"
#include "queue.h"
#include "device.h"

extern list_node_t tasks_wait;
static void keypress_isr(registers_t *regs);

queue_t * keybd_queue;

device_t keybd_device;



/* KBDUS means US Keyboard Layout. This is a scancode table
*  used to layout a standard US keyboard. I have left some
*  comments in to give you an idea of what key is what, even
*  though I set it's array index to 0. You can change that to
*  whatever you want using a macro, if you wish! */
static unsigned char kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', /* 9 */
  '9', '0', '-', '=', '\b', /* Backspace */
  '\t',         /* Tab */
  'q', 'w', 'e', 'r',   /* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
    0,          /* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', /* 39 */
 '\'', '`',   0,        /* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',            /* 49 */
  'm', ',', '.', '/',   0,              /* Right shift */
  '*',
    0,  /* Alt */
  ' ',  /* Space bar */
    0,  /* Caps lock */
    0,  /* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,  /* < ... F10 */
    0,  /* 69 - Num lock*/
    0,  /* Scroll Lock */
    0,  /* Home key */
    0,  /* Up Arrow */
    0,  /* Page Up */
  '-',
    0,  /* Left Arrow */
    0,
    0,  /* Right Arrow */
  '+',
    0,  /* 79 - End key*/
    0,  /* Down Arrow */
    0,  /* Page Down */
    0,  /* Insert Key */
    0,  /* Delete Key */
    0,   0,   0,
    0,  /* F11 Key */
    0,  /* F12 Key */
    0,  /* All other keys are undefined */
};

static int isalpha(int c) {
    return ((unsigned int)(c | 32) - 97) < 26U;
}

static int isprintable(int c) {
    return (c >= 32 && c <= 126);
}



static void keypress_isr(registers_t *regs) {
    static uint32_t scancode;
    
    scancode = inb(0x60);
    if (!(scancode&0x80)) {
        asm volatile("cli");
        queue_send(keybd_queue, &scancode, QM_NONBLOCKING);
        asm volatile("sti");
    }
}

void* console_device(void * arg) {

    uint32_t scancode;
    iorq_t iorq;
    int i;
    char c;
    char * aux;
    
    memset(&keybd_device, 0, sizeof(device_t));
    strcpy(keybd_device.ln_link.name, "org.era.dev.console");
    keybd_device.iorq_queue = create_queue(10, sizeof(iorq_t));

    keybd_queue = create_queue(1, sizeof(uint32_t));
    register_interrupt_handler(IRQ1, &keypress_isr);
    monitor_writexy(0,24, " 1", 7, 0);
    
    for(;;) {
        queue_recv(keybd_device.iorq_queue, &iorq, QM_BLOCKING);
        if(iorq.io_desc == DC_READ) {
            i = 0;
            aux = (char *) iorq.io_dptr;
            memset(aux, 0, iorq.io_sz);
            
            while(1) {
                queue_recv(keybd_queue, &scancode, QM_BLOCKING);
                c = kbdus[scancode];
                if (c == '\n')
                    break;
                if ((c == '\b') && (i > 0)) {
                    aux[i] = '\0';
                    i--;
                    monitor_put('\b');
                    continue;
                }
                if (c == 0 || c == '\t' || i == (iorq.io_sz - 1))
                    continue;
                if(isprintable(c) ) {
                    aux[i++] = c;
                    monitor_put(c);
                }
            }
            monitor_put('\n');
            aux[i] = '\0';

            queue_send(iorq.io_response, NULL, QM_NONBLOCKING);
        }
    }
}
