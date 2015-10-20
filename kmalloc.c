/*! \file kmalloc.c */

/* Krypton OS kernel heap implementation

   Description: This file implements the kmalloc()/kfree()
   interface to be used in Krypton OS, using a free list
   mechanism with automated memory chunk coalescence and
   integrity testing.
   
   TODO: Implement deferred merge_heap() calls, eg. in the
   idle task, to allow constant-time freeing. */

#include "kmalloc.h"
#include "syscalls.h"
#include "mm.h"

/* Virtual memory heap starting address */
#define HEAP_START       (unsigned long)     0xC0400000

list_head_t k_heap_start;
uint32_t k_heap_end;

/* Static prototypes */

static void insert_chunk(chunk_t * chunk);
static void merge_heap();
static void merge_heap(chunk_t *aux);


/* kkeap_init()
   Description: This function initialises the kernel heap
   
   Parameters: none
   Returns: none
   Notes:
   TODO: set here the system calls!
*/
void
kheap_init () {
    new_list((list_head_t *) &k_heap_start);
    k_heap_end = HEAP_START;
}

void
kheap_traverse () {
    chunk_t *chunk = (chunk_t *) get_head((list_head_t *) &k_heap_start);
    int i = 1;
    
    if (!chunk) {
        kprintf("Heap is empty!\n\n");
        return;
    }
    
    kprintf("\n---- Kernel Heap Traversal ---- \n");
    kprintf(" No     ADDR         SIZE     MAGIC \n");
    
    while(chunk) {
        kprintf("%3d   0x%08X %8d     0x%08X. \n", 
                i++, (uint32_t) chunk, chunk->size, chunk->magic);
        chunk = (chunk_t *) get_next((list_node_t *) chunk);
    }
}

void
_kfree (void* ptr)
{
    uint32_t chunk_addr = (uint32_t) ptr;
    chunk_t * chunk;
    
    chunk_addr -= sizeof(chunk_t);
    chunk = (chunk_t*) chunk_addr;
    
    if(chunk->magic != CHUNK_MAGIC) {
			panic("kernel heap corruption detected");
    }
    
    asm volatile("cli");
    insert_chunk(chunk);
    merge_heap(get_head((list_head_t *) &k_heap_start));
    asm volatile("sti");
}

void kfree(void * ptr) {
    system_call(SYSCALL_KFREE, ptr);
}

void * kmalloc(uint32_t alloc_sz) {
    uint32_t aux = alloc_sz;
    system_call(SYSCALL_KMALLOC, &aux);
    
    return aux;
}

void*
_kmalloc (uint32_t alloc_sz) {

	chunk_t *chunk = (chunk_t *) get_head((list_head_t *) &k_heap_start);
	chunk_t *next_chunk, *new_chunk;
	uint32_t next_chunk_addr, chunk_addr, new_chunk_addr;
	uint32_t heap_end_addr = k_heap_end;
	/* Allocation size has to be properly aligned */
	if (alloc_sz & 0xF)
	    alloc_sz = (alloc_sz + 0x10) & (~0xF);
	/* Try to find a suitable chunk in the list.
	   If no one is found, we need to expand the heap */

	while(chunk) {
		/* Check the chunk for consistency, i.e. if the struct
		   was not been overwritten */
		if(chunk->magic != CHUNK_MAGIC) {
			panic("kernel heap corruption detected");
		}
		/* See if the chunk fits widely. If not, get the next one and iterate */
		if(chunk->size < alloc_sz * 2) {
			chunk = (chunk_t *) get_next((list_node_t *) chunk);
			continue;
		}
		/* A suitable chunk was found - let's resize it */
		/* First check if the resizing is worth for, i.e. if there will be space
		   for the next chunk header */
		/* We will typecast the pointers to integer type, to be able to do pointer
		   arithmetic correctly */
		asm volatile("cli");
		chunk_addr = (uint32_t) chunk;
		/*  Two cases to watch for: if we are the last node or not */
		if(next_chunk = (chunk_t *) get_next((list_node_t *) chunk)) {
			/* We have a node in front of us */
			/* Check again for heap consistency */
			if(next_chunk->magic != CHUNK_MAGIC) {
				panic("kernel heap corruption detected");
			}
			/* Typecast to integer to ease the math */
			next_chunk_addr = (uint32_t) next_chunk;
			/* Test if we need to split the chunk and resize it */
			if(alloc_sz >= chunk->size - sizeof(chunk_t)) {
				/* Resize not worthy. Just set the size of this chunk accordingly */
				
				//chunk->size = next_chunk_addr - chunk_addr - sizeof(chunk_t);
			}
			else {
				/* We must do a resize, so create a node in front of this chunk and link it */
				new_chunk_addr = chunk_addr + sizeof(chunk_t) + alloc_sz;
				new_chunk = (chunk_t *) new_chunk_addr;
				/* Compute the size of the new node accordingly*/
				new_chunk->size = chunk->size - alloc_sz - sizeof(chunk_t);
				/* Set the magic number, as always */
				new_chunk->magic = CHUNK_MAGIC;
				/* Enqueue the new chunk */
				insert_chunk(new_chunk);
				/* Reset the size of this chunk */
				chunk->size = alloc_sz;
			}
		}
		else {
			/* We are the last chunk in the heap, so test the limits 
			   against the heap end address */
			if(alloc_sz >= chunk->size - sizeof(chunk_t)) {
				/* Resize not worthy, do as told before */
				//chunk->size = heap_end_addr - chunk_addr - sizeof(chunk_t);
			}
			else{
				/* Here we must do a resize as described before */
				new_chunk_addr = chunk_addr + sizeof(chunk_t) + alloc_sz;
				new_chunk = (chunk_t *) new_chunk_addr;
				/* Compute the size of the new node accordingly*/
				new_chunk->size = chunk->size - alloc_sz - sizeof(chunk_t);
				/* Set the magic number, as always */
				new_chunk->magic = CHUNK_MAGIC;
				/* Enqueue the new chunk */
				insert_chunk(new_chunk);
				/* Reset the size of this chunk */
				chunk->size = alloc_sz;
			}
		}
		/* Remove the chunk from the free list */
		remove(chunk);
		/* We finished setting up the chunks, so all we need to do
		   is to return the new chunk address to the caller! */
		asm volatile("sti");

		return ((void*) (chunk_addr + sizeof(chunk_t)));
	}
	/* If we get here, we need to allocate more heap space */
	/* We will ask for a new 4 kiB page from the virtual memory
	   allocator and map it in front of the current heap end */
	/* Then we simply resize the last free chunk and recursively
	   reinvoke the allocator */
	asm volatile("cli");
	mm_map(pa_alloc(), heap_end_addr, PAGE_WRITE | PAGE_USER);
	/* Reset the heap end address */
	k_heap_end += PAGE_SIZE;
	/* Get last chunk from list */
	new_chunk = (chunk_t *) get_tail((list_head_t *) &k_heap_start);
	/* Test if the heap was empty before */
	if(new_chunk) {
		/* If the heap exists, resize the chunk  */
		new_chunk->size += PAGE_SIZE;
	} else {
		/* We need to create a chunk in the heap end
		   and add it to the free chunks list */
		new_chunk = (chunk_t*) heap_end_addr;
		/* Set it's size */
		new_chunk->size = k_heap_end - heap_end_addr - sizeof(chunk_t);
		/* Set the magic number, as always */
		new_chunk->magic = CHUNK_MAGIC;
		/* Enqueue the new chunk */
		add_tail(&k_heap_start, new_chunk);
	}
	/* Recurse to retry the allocation */
	asm volatile("sti");
	return ( _kmalloc(alloc_sz) );
}

static void insert_chunk(chunk_t * chunk) {
    chunk_t *aux = (chunk_t *) get_head((list_head_t *) &k_heap_start);
    uint32_t aux_addr = (uint32_t) aux;
    
    /* First iterate through the list to order by address */
    while( (aux != NULL) &&
           (aux_addr < (uint32_t) chunk) ) {
        aux = (chunk_t *) get_next((list_node_t*) aux);
        aux_addr = (uint32_t) aux;
    }
    if(aux == NULL) {
        /* If we came to the end of the list, simply insert at the end */
        add_tail((list_head_t *) &k_heap_start, (list_node_t*) chunk);
    }
    else {
        /* If not, we will insert BEFORE the auxilliary pointer */
        chunk->mn_link.next = (list_node_t*) aux;
        chunk->mn_link.prev = (list_node_t*) aux->mn_link.prev;
        aux->mn_link.prev = (list_node_t*) chunk;
        chunk->mn_link.prev->next = (list_node_t*) chunk;
    }  
}

static void merge_heap(chunk_t *aux) {
    chunk_t *aux_next;
    chunk_t * tail;
    uint32_t aux_addr = (uint32_t) aux;
    uint32_t aux_next_addr;
    uint32_t free_addr;
    
    if (aux == NULL) return;
    
    if(aux->magic != CHUNK_MAGIC) {
	    panic("kernel heap corruption detected");
	}

    aux_next = (chunk_t *) get_next((list_node_t *) aux);
    aux_next_addr = (uint32_t) aux_next;
    
    if( aux_next_addr == aux_addr + aux->size + sizeof(chunk_t) ) {
        aux->size += aux_next->size + sizeof(chunk_t);
        remove(aux_next);
        merge_heap(aux);
    }
    else {
        merge_heap(get_next(aux));
    }
    
    tail = get_tail(&k_heap_start);
    while((tail != NULL) && (tail->size > PAGE_SIZE)) {
        tail->size -= PAGE_SIZE;
        k_heap_end -= PAGE_SIZE;
        free_addr = get_physaddr(k_heap_end);
        mm_unmap(k_heap_end);
        pa_free(free_addr);
    }
}

