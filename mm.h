/* 
 * File:   mm.h
 * Author: renato
 *
 * Created on November 15, 2013, 4:10 PM
 */

#ifndef _MM_H
#define	_MM_H

#include "common.h"
#include "multiboot.h"


#define PAGE_PRESENT   0x1        // Page is mapped in.
#define PAGE_WRITE     0x2        // Page is writable. Not set means read-only.
#define PAGE_USER      0x4        // Page is writable from user space. Unset means kernel-only.
#define PAGE_MASK      0xFFFFF000 // Mask constant to page-align an address.
#define PAGE_SIZE	   4096

typedef unsigned long pagedir_t;
typedef unsigned long pagetable_t;

void mm_init(multiboot_t * mboot);

unsigned int pa_alloc();

void pa_free(unsigned int page);

void * get_physaddr(void * virtualaddr);

void * mm_map(void * physaddr, void * virtualaddr, unsigned int flags);

void mm_unmap(void * virtualaddr);

void switch_page_directory(void *pagetabledir_ptr);

void * dos_mm_map(void * physaddr, void * virtualaddr, unsigned int flags);

void dos_mm_unmap(void * virtualaddr);

#endif	/* _PMM_H */

