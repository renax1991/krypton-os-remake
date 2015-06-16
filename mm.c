/* mm.c - Krypton memory management functions
 *
 * - [RGE] Code based on code from JamesM tutorials (2013-??-??)
 * - [RGE] Full support for kernel heap ops added (2014-02-23)
 */

#include "mm.h"
#include "cpu.h"
#include "monitor.h"
#include "common.h"
#include "panic.h"
#include "kprintf.h"
#include "idt.h"
#include "syscalls.h"
 
 
#define PM_DIR_CLONE_ADDR   (unsigned long *)   0xFE000000
#define PM_PAGE_CLONE_ADDR  (unsigned long *)   0xFE800000
#define PM_LOW_PAGE_COUNT                       120


/***************************************
 * Static Function Prototypes
 ***************************************/

static inline void flush_tlb(unsigned long virtualaddr);

static void page_fault(registers_t *regs);

/***************************************
 * Globals
 ***************************************/
/* x86-compatible Page Directory container */
unsigned long kernelpagedir[1024] __attribute__((aligned(4096)));
/* x86-compatible 0-4MiB Page Table container */
unsigned long lowpagetable[1024] __attribute__((aligned(4096)));
/* Pointer to the page directory phisical address */
void *kernelpagedirPtr = 0;
/* Last kernel page physical address */
uint32_t kernel_seg_end_page;
/* flag to tell if the virtual memory system is initialized */
unsigned int vm_online;
/* Constant defined in the linker script to mark the kernel area end */
extern unsigned int end;

/* Low-end RAM (<15 MB) free-pages bitfield */
uint32_t LowRamBitF[PM_LOW_PAGE_COUNT];
uint32_t LowRamFreeCount;

/* High end RAM counter */
uint32_t HighRamFreeCount;

static void set_page_bit(uint32_t paddr) {
    uint32_t page_no = paddr >> 12; // (paddr / 4096)
    uint32_t bf_idx = page_no >> 5; // (page_no / 32)
    uint32_t bit_mask = 1 << (page_no & 0x1F); // (page_no % 32)
    
    /* Clear the bit in the bitfield */
    LowRamBitF[bf_idx] &= (~bit_mask);
    /* Decrement the free pages counter */
    LowRamFreeCount--;
}

static int tst_page_bit(uint32_t paddr) {
    uint32_t page_no = paddr >> 12; // (paddr / 4096)
    uint32_t bf_idx = page_no >> 5; // (page_no / 32)
    uint32_t bit_mask = 1 << (page_no & 0x1F); // (page_no % 32)
    
    /* Test the bit in the bitfield */
    return (LowRamBitF[bf_idx] & bit_mask);
}

static void clr_page_bit(uint32_t paddr) {
    uint32_t page_no = paddr >> 12; // (paddr / 4096)
    uint32_t bf_idx = page_no >> 5; // (page_no / 32)
    uint32_t bit_mask = 1 << (page_no & 0x1F); // (page_no % 32)
    
    /* Set the bit in the bitfield */
    LowRamBitF[bf_idx] |= bit_mask;
    /* Increment the free pages counter */
    LowRamFreeCount++;
}

static void dump_registers(registers_t * regs) {
    kprintf("\nx86 register state dump\n");
    kprintf("CS: 0x%04X EIP: 0x%08X EFLAGS: 0x%08X\n", regs->cs, regs->eip, regs->eflags);
    kprintf("EAX: 0x%08X EBX: 0x%08X ECX: 0x%08X EDX: 0x%08X\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("ESI: 0x%08X EDI: 0x%08X EBP: 0x%08X ESP: 0x%08X\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("DS: 0x%04X\n", regs->ds);
}

static void protection_fault(registers_t *regs) {
    uint32_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2));
    dump_registers(regs);
    panic("protection fault in kernel space");
}

uint32_t pa_alloc() {
    uint32_t idx = 0;
    uint32_t bitmask = 1, bit_no = 0;
    uint32_t paddr = 0;
    
    if(LowRamFreeCount <= 0)
        panic("out of memory.");
    /* Speed up the search by skipping full zones */
    while( LowRamBitF[idx] == 0 ) idx++;
    /* Now find the first free page in the field */
    while( (LowRamBitF[idx] & bitmask) == 0 ) {
        bitmask = bitmask << 1;
        bit_no++;
    }
    /* Calculate the physical address of the free page */
    paddr = ((idx * 32) + bit_no) << 12;
    /* Set the page as non-free and return */
    set_page_bit(paddr);
    return paddr;
}

void pa_free(uint32_t paddr) {
    if(!tst_page_bit(paddr))
        clr_page_bit(paddr);
}

/* init_paging() - paging system bootstrap
 *
 * This function fills the page directory and the page table,
 * then enables paging by putting the address of the page directory
 * into the CR3 register and setting the 31st bit into the CR0 one
 */
void init_paging() {
    // Pointers to the page directory and the page table

    void *lowpagetablePtr = 0;
    int k = 0;

    // Translate the page directory from
    // virtual address to physical address
    kernelpagedirPtr = (char *) kernelpagedir + 0x40000000;
    // Same for the page table
    lowpagetablePtr = (char *) lowpagetable + 0x40000000;

    // Counts from 0 to 1023 to...
    for (k = 0; k < 1024; k++) {
        // ...map the first 4MB of memory into the page table...
        lowpagetable[k] = (k * 4096) | 0x7;
        // ...and clear the page directory entries
        kernelpagedir[k] = 0;
    }

    // Fills the addresses 0...4MB and 3072MB...3076MB
    // of the page directory with the same page table

    kernelpagedir[0] = ((unsigned long) lowpagetablePtr) | 0x7;
    kernelpagedir[768] = ((unsigned long) lowpagetablePtr) | 0x7;

    // Self-reference the page directory for later easy access
    kernelpagedir[1023] = ((unsigned long) kernelpagedirPtr) | 0x7;

    // Copies the address of the page directory into the CR3 register and,
    // finally, enables paging!

    asm volatile ( "mov %0, %%eax\n"
            "mov %%eax, %%cr3\n"
            "mov %%cr0, %%eax\n"
            "orl $0x80000000, %%eax\n"
            "mov %%eax, %%cr0\n" ::"m" (kernelpagedirPtr));
}

void switch_page_directory(void *pagetabledir_ptr) {
    asm volatile ( "mov %0, %%eax\n"
            "mov %%eax, %%cr3\n" ::"m" (pagetabledir_ptr));
}

/* mm_init() - memory manager initialization function
 *
 * This calls init_paging to rudely map some space into the PD,
 * places sys_base into place and bootstraps the page allocator.
 * Then it sets up the kernel memory heap and maps sys_base to it's
 * beginning.
 */
void mm_init(multiboot_t *mboot_ptr) {
    int i;
    
    // Calculate the kernel end physical address
    unsigned int kernel_end = ((unsigned int) &end) - 0xC0000000;
    // initialize paging, get rid of segmentation and initialize interrupts
    init_paging();
    gdt_install();
    init_idt();

    // Register the page-fault handler (Exp. 14 on x86)
    register_interrupt_handler(14, &page_fault);
    register_interrupt_handler(13, &protection_fault);
    
    /* Initialize the low-memory page allocator */
    for (i = 0; i < PM_LOW_PAGE_COUNT; i++)
        LowRamBitF[i] = 0x0;
    /* Set the low-memory free page count */
    LowRamFreeCount = 0;
    
    /* Map the free memory map */
    i = mboot_ptr->mmap_addr;
    while (i < mboot_ptr->mmap_addr + mboot_ptr->mmap_length) {
        mmap_entry_t *me = (mmap_entry_t*) i;
        // Does this entry specify usable RAM?
        if (me->type == 1) {
            uint32_t j;
            // For every page in this entry, add to the free page stack.
            for (j = me->base_addr_low; j < me->base_addr_low + me->length_low; j += 0x1000) {
                if(j >= 0xF00000)
                    break;
                pa_free(j);
            }
        }

        // The multiboot specification is strange in this respect - 
        // the size member does not include "size" itself in its calculations,
        // so we must add sizeof (uint32_t).
        i += me->size + sizeof (uint32_t);
    }
    /* lock the BIOS data area */
    /* First the BDA */
    set_page_bit(0x0);
    /* For now, high RAM will not be managed */
    HighRamFreeCount = 0;
    /* Set the kernel area as being used, and we're done */
    for(i = 0x100000; i < kernel_end; i += 0x1000)
        set_page_bit(i);
        
    mm_unmap(0x0);
}

void * get_physaddr(void * virtualaddr) {
    unsigned long pdindex = (unsigned long) virtualaddr >> 22;
    unsigned long ptindex = (unsigned long) virtualaddr >> 12 & 0x03FF;

    unsigned long * pd = (unsigned long *) 0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // If the page table isn't present, return null
    if ((pd[pdindex] & 0x01) == 0)
        return NULL;

    unsigned long * pt = ((unsigned long *) 0xFFC00000) + (0x400 * pdindex);
    // Here you need to check whether the PT entry is present.
    // If the page isn't present, return null
    if ((pt[ptindex] & 0x01) == 0)
        return NULL;

    return (void *) ((pt[ptindex] & ~0xFFF) + ((unsigned long) virtualaddr & 0xFFF));
}

void * dos_mm_map(void * physaddr, void * virtualaddr, unsigned int flags){
    struct {
        void * physaddr;
        void * virtualaddr;
        unsigned int flags;
    } args;
    
    args.physaddr = physaddr;
    args.virtualaddr = virtualaddr;
    args.flags = flags;
    system_call(SYSCALL_MMMAP, &args);
    
    return args.virtualaddr;
}

void dos_mm_unmap(void * virtualaddr){
    system_call(SYSCALL_MMUNMAP, virtualaddr);
}

void * mm_map(void * physaddr, void * virtualaddr, unsigned int flags) {

    unsigned long pdindex = (unsigned long) virtualaddr >> 22;
    unsigned long ptindex = ((unsigned long) virtualaddr >> 12) & 0x03FF;
    unsigned long * pt;

    if (physaddr == NULL)
        return 0;

    unsigned long * pd = (unsigned long *) 0xFFFFF000;
    // Here you need to check whether the PD entry is present.
    // When it is not present, you need to create a new empty PT and
    // adjust the PDE accordingly.

    if ((pd[pdindex] & 0x01) == 0){ // If the page table isn't present, add one
        pd[pdindex] = pa_alloc() | (flags & 0xFFF) | 0x01;
        // zero out the page table
        pt = ((unsigned long *) 0xFFC00000) + 0x400 * pdindex;
        memset(pt, 0, 4096);
    }

    pt = ((unsigned long *) 0xFFC00000) + 0x400 * pdindex; // 0x400 ??
    // Here you need to check whether the PT entry is present.
    // When it is, then there is already a mapping present. What do you do now?
    pt[ptindex] = ((unsigned long) physaddr) | (flags & 0xFFF) | 0x01; // Present

    // Now you need to flush the entry in the TLB
    // to validate the change.

    flush_tlb((unsigned long) virtualaddr);

    return virtualaddr;
}

void mm_unmap(void * virtualaddr) {

    unsigned long pdindex = (unsigned long) virtualaddr >> 22;
    unsigned long ptindex = ((unsigned long) virtualaddr >> 12) & 0x03FF;
    unsigned long * pt;
    
    pt = ((unsigned long *) 0xFFC00000) + 0x400 * pdindex; // 0x400 ??
    // Set the page table entry to 0
    pt[ptindex] = 0;

    // Now you need to flush the entry in the TLB
    // to validate the change.

    flush_tlb((unsigned long) virtualaddr);
}

static void page_fault(registers_t *regs) {
    uint32_t cr2;
    asm volatile ("mov %%cr2, %0" : "=r" (cr2));

    kprintf("\nPage fault at 0x%x, faulting address 0x%x\n", regs->eip, cr2);
    kprintf("CPU error code: %x\n", regs->err_code);
    
    //kprintf("OOPS: %s crashed at 0x%x!!\n", sys_base->running_thread->node.name, regs->eip);
    kprintf("CS: 0x%4X EIP: 0x%8X EFLAGS: 0x%8X\n", regs->cs, regs->eip, regs->eflags);
    kprintf("EAX: 0x%8X EBX: 0x%8X ECX: 0x%8X EDX: 0x%8X\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("ESI: 0x%8X EDI: 0x%8X EBP: 0x%8X ESP: 0x%8X\n", regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("DS: 0x%4X\n", regs->ds);
    panic("unexpected page fault.");
    for (;;);
}

// Clones the current page directory
// Returns the physycal address of the new directory
pagedir_t * clone_actual_directory(){
    unsigned long * source_pd = (unsigned long *) 0xFFFFF000;
    unsigned long * dest_pd;
    pagedir_t * dest_pd_physical = pa_alloc();
    pagetable_t * dest_pt_physical;
    unsigned long * pt;
    int i;
    // Copy first the page directory
    dest_pd = mm_map(dest_pd_physical, PM_DIR_CLONE_ADDR, PAGE_WRITE);
    memcpy(PM_DIR_CLONE_ADDR, source_pd, 4096);
    // Now iterate over each one of the page tables
    // and copy those who exist
    for(i = 0; i < 1024; i++){
        pt = ((unsigned long *) 0xFFC00000) + (0x400 * i);
        if (pt != 0){
            dest_pt_physical = pa_alloc();
            mm_map(dest_pt_physical, PM_DIR_CLONE_ADDR, PAGE_WRITE);
            memcpy(PM_DIR_CLONE_ADDR, ((unsigned long)pt & PAGE_MASK), 4096);
            // Point the directory entry to the new page 
            // with the same permitions
            dest_pd[i] = ((unsigned long)pt & 0x7) |
                    (unsigned long)dest_pt_physical;
        }
    }
    return dest_pd_physical;
}

/* flush_tlb(virtualaddr) - TLB entry invalidation function
 *
 * Came directly from the Linux kernel
 */
static inline void flush_tlb(unsigned long virtualaddr) {
    asm volatile("invlpg (%0)" ::"r" (virtualaddr) : "memory");
}


