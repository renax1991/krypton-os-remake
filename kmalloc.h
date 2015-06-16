/*! \file kmalloc.h */

#ifndef _KMALLOC_H
#define _KMALLOC_H

#include "common.h"
/*!
 * A magic number to ease checking heap consistency
 */
#define CHUNK_MAGIC   	0xABADBABE

/*!
 * Kernel heap starting address
 */
#define HEAP_ADDR		0xC0400000

/*!
 * The basic chunk structure, linkable in a list
 * ordered by address
 */
struct chunk_s {
	min_node_t mn_link;  //! Link to other chunks in a ordered list
	uint32_t size;       //! Size of this chunk
	uint32_t magic;      //! Magic number for consistency checking
};

typedef struct chunk_s chunk_t;

void
kheap_traverse ();

void
kheap_init ();

void*
kmalloc (uint32_t alloc_sz);

void*
_kmalloc (uint32_t alloc_sz);

void
kfree (void* ptr);

void
_kfree (void* ptr);

#endif /* _KMALLOC_H */
