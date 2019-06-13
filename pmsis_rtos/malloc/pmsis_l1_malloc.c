#include "pmsis_l1_malloc.h"
#include "pmsis_task.h"

#include <string.h>
#include <stdio.h>

malloc_t __l1_malloc;
pmsis_mutex_t __l1_malloc_mutex;

void *pmsis_l1_malloc(uint32_t size)
{
    void *ret_ptr;
    ret_ptr = __malloc(&__l1_malloc, size);
    return ret_ptr;
}

void pmsis_l1_malloc_free(void *_chunk, int size)
{
    __malloc_free(&__l1_malloc, _chunk, size);
}

void *pmsis_l1_malloc_align(int size, int align)
{
    void *ret_ptr;
    ret_ptr = __malloc_align(&__l1_malloc, size, align);
    return ret_ptr;
}

/** \brief malloc init: initialize mutex and heap
 * Warning: thread unsafe by nature
 * \param heapstart heap start address
 * \param heap_size size of the heap
 */
void pmsis_l1_malloc_init(void *heapstart, uint32_t heap_size)
{
    __malloc_init(&__l1_malloc, heapstart, heap_size);
}

void pmsis_l1_malloc_set_malloc_t(malloc_t malloc_struct)
{
    memcpy(&__l1_malloc,&malloc_struct,sizeof(malloc_t));
}

malloc_t pmsis_l1_malloc_get_malloc_t(void)
{
    malloc_t malloc_struct;
    memcpy(&malloc_struct,&__l1_malloc,sizeof(malloc_t));   
    return malloc_struct;
}
