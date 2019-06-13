#include <string.h>
#include <stdio.h>
#include "pmsis_hyperram_malloc.h"
#include "pmsis_task.h"

static malloc_t __hyperram_malloc;
static pmsis_spinlock_t __hyperram_malloc_spinlock;

void *pmsis_hyperram_malloc(int32_t size)
{
    void *ret_ptr;
    pmsis_spinlock_take(&__hyperram_malloc_spinlock);
    ret_ptr = __malloc_extern(&__hyperram_malloc, size);
    pmsis_spinlock_release(&__hyperram_malloc_spinlock);
    return ret_ptr;
}

void pmsis_hyperram_malloc_free(void *_chunk, int32_t size)
{
    pmsis_spinlock_take(&__hyperram_malloc_spinlock);
    __malloc_extern_free(&__hyperram_malloc, _chunk, size);
    pmsis_spinlock_release(&__hyperram_malloc_spinlock);
}

/** \brief malloc init: initialize mutex and heap
 * Warning: thread unsafe by nature
 * \param heapstart heap start address
 * \param heap_size size of the heap
 */
void pmsis_hyperram_malloc_init(void *heapstart, int32_t heap_size)
{
    __malloc_extern_init(&__hyperram_malloc, heapstart, heap_size);
    pmsis_spinlock_init(&__hyperram_malloc_spinlock);
}

void pmsis_hyperram_malloc_deinit()
{
    __malloc_extern_deinit(&__hyperram_malloc);
}
