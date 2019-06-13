#if 1
#include <stdlib.h>

#include "event_kernel.h"
#include "pmsis_types.h"
#include "pmsis_task.h"
#include "cl_synchronisation.h"
#include "fc_to_cl_delegate.h"
#include "core_utils.h"
#include "pmsis_eu.h"

#include "pmsis_l1_malloc.h"

/* Use pmsis malloc instead of GCC or other malloc. */
#define malloc pmsis_malloc

#define PRINTF(...) ((void)0)

struct pmsis_event_kernel_wrap *default_sched;

/** Internal structs for the event kernel **/
typedef struct pmsis_event {
    struct pmsis_event *next;
    struct pmsis_event_scheduler *sched;
    pi_task_t *fc_task;
} pmsis_event_t;

/**
 * event scheduler structure:
 * give a fifo for active events and a list of free events which can be used
 * by event push at any time
 **/
typedef struct pmsis_event_scheduler {
    struct pmsis_event *first;
    struct pmsis_event *last;
    struct pmsis_event *first_free;
    int32_t allocated_event_nb;
    int32_t max_event_nb;
} pmsis_event_scheduler_t;

struct pmsis_event_kernel
{
    spinlock_t cl_to_fc_spinlock;
    pmsis_mutex_t event_sched_mutex;
    struct pmsis_event_scheduler *scheduler;
    int running;
};

static inline int pmsis_event_lock_cl_to_fc(struct pmsis_event_kernel *evt_kernel)
{
    int irq = 0;
    if(__native_is_fc())
    {
        if(pi_cluster_is_on())
        {
            cl_sync_spinlock_take(&evt_kernel->cl_to_fc_spinlock);
        }
        else
        {
            irq = disable_irq();
        }
    }
    else
    {
        cl_sync_spinlock_take(&(evt_kernel->cl_to_fc_spinlock));
    }
    return irq;
}

static inline void pmsis_event_unlock_cl_to_fc(struct pmsis_event_kernel *evt_kernel,
        int irq_prev)
{
    if(__native_is_fc())
    {
        if(pi_cluster_is_on())
        {
            cl_sync_spinlock_release(&evt_kernel->cl_to_fc_spinlock);
        }
        else
        {
            restore_irq(irq_prev);
        }
    }
    else
    {
        cl_sync_spinlock_release(&(evt_kernel->cl_to_fc_spinlock));
    }
}

static inline struct pmsis_event_kernel *pmsis_event_wrap_get_kernel(
        struct pmsis_event_kernel_wrap *wrap)
{
    struct pmsis_event_kernel *kern = (struct pmsis_event_kernel*)wrap->priv;
    return kern;
}


void pmsis_event_lock_cl_to_fc_init(struct pmsis_event_kernel_wrap *wrap)
{
    struct pmsis_event_kernel *evt_kernel = pmsis_event_wrap_get_kernel(wrap);
    cl_sync_init_spinlock(&(evt_kernel->cl_to_fc_spinlock), pmsis_l1_malloc(sizeof(uint32_t)));
    hal_compiler_barrier();
}

static inline void pmsis_event_wrap_set_kernel(
        struct pmsis_event_kernel_wrap *wrap,
        struct pmsis_event_kernel *event_kernel)
{
    wrap->priv = (void*)event_kernel;
}

static inline struct pmsis_event_scheduler *pmsis_event_wrap_get_scheduler(
        struct pmsis_event_kernel_wrap *wrap)
{
    struct pmsis_event_kernel *kern = pmsis_event_wrap_get_kernel(wrap);
    return kern->scheduler;
}

static inline void pmsis_event_wrap_set_scheduler(
        struct pmsis_event_kernel_wrap *wrap,
        struct pmsis_event_scheduler *sched)
{
    struct pmsis_event_kernel *kern = pmsis_event_wrap_get_kernel(wrap);
    kern->scheduler = sched;
}

void pmsis_event_kernel_mutex_release(struct pmsis_event_kernel_wrap *wrap)
{
    struct pmsis_event_kernel *kern = pmsis_event_wrap_get_kernel(wrap);
    pmsis_mutex_t *sched_mutex = &(kern->event_sched_mutex);
    pmsis_mutex_release(sched_mutex);
}

/**
 * Release an active event from the FIFO list
 * and push it on the free list
 * if allocated_event_nb > max_event_nb the event will be freed instead
 */
static inline void pmsis_event_release(struct pmsis_event *event);

/**
 * pop an event from the event FIFO, returns NULL if none available
 * MAY SLEEP
 */
static inline void pmsis_event_pop(struct pmsis_event_kernel *event_kernel,
        struct pmsis_event **event);

/**
 * Get an event from the scheduler free list
 * Returns NULL if none available
 */
static inline struct pmsis_event *pmsis_event_get(struct pmsis_event_scheduler *sched);

/**** Actual implementations ****/

/**
 * Take an event from free event list, and make it usable by an event push
 *
 * mutex with pmsis_event_alloc
 */
static inline struct pmsis_event *pmsis_event_get(struct pmsis_event_scheduler *sched)
{
    // considering how short it is, we can afford to disable irqs
    int irq = disable_irq();
    struct pmsis_event *event;
    if(!sched->first_free)
    {
        PRINTF("Can't get a free event\n");
        return NULL;
    }
    event = sched->first_free;
    sched->first_free = sched->first_free->next;

    event->next = NULL; // for now, no known next event
    event->sched = sched;
    restore_irq(irq);
    return event;
}


int pmsis_event_free(struct pmsis_event_kernel_wrap *wrap, int nb_events)
{
    struct pmsis_event_scheduler *sched = pmsis_event_wrap_get_scheduler(wrap);
    if(sched->max_event_nb >= nb_events)
    {
        sched->max_event_nb -= nb_events;
    }
    else
    {
        sched->max_event_nb = 0;
        PRINTF("free more events than there really are");
    }

    int nb_freed_events = 0;
    struct pmsis_event *free_event;
    while((nb_freed_events < nb_events) && (free_event = pmsis_event_get(sched)))
    {
        free(free_event);
        sched->allocated_event_nb--;
    }
    return nb_freed_events;
}

static inline void pmsis_event_release(struct pmsis_event *event)
{
    struct pmsis_event_scheduler *sched = event->sched;

    PRINTF("rt_event_release: BEFORE: sched->first_free: %p\n",sched->first_free);

    PRINTF("rt_event_release: event_ptr is %p\n",event);
    
    event->fc_task->done = 1;
    pmsis_mutex_release(&event->fc_task->wait_on);
    event->fc_task = NULL;

    if(sched->allocated_event_nb> sched->max_event_nb)
    {
        free(event);
    }
    else
    {
        event->next = sched->first_free;
        sched->first_free = event;
    }

    PRINTF("rt_event_release: AFTER: sched->first_free: %p\n",sched->first_free);
}

/** allocate a given number of events in event_kernel free list
 * returns number of really allocated events
 **/
int pmsis_event_alloc(struct pmsis_event_kernel_wrap *wrap, int nb_events)
{
    struct pmsis_event_kernel *event_kernel = pmsis_event_wrap_get_kernel(wrap);
    struct pmsis_event *alloc_event;
    struct pmsis_event_scheduler *sched = pmsis_event_wrap_get_scheduler(wrap);

    if(sched->allocated_event_nb > sched->max_event_nb)
    {
        // if a free as not been fully executed yet, don't allow unecessary
        // events
        nb_events -=(sched->allocated_event_nb - sched->max_event_nb);
    }

    int nb_allocated_event = 0;
    for(int i=0;i<nb_events;i++)
    {
        alloc_event = (struct pmsis_event*)malloc(sizeof(struct pmsis_event));
        if(alloc_event == NULL)
        {
            return nb_allocated_event;
        }
        PRINTF("Alloc event ptr: %p\n",alloc_event);
        int irq = pmsis_event_lock_cl_to_fc(event_kernel);
        // ----- critical section (manipulating free list)
        alloc_event->fc_task = NULL;
        alloc_event->next = sched->first_free;
        sched->first_free = alloc_event;
        sched->allocated_event_nb++;
        sched->max_event_nb++;
        nb_allocated_event++;
        // -----
        pmsis_event_unlock_cl_to_fc(event_kernel, irq);
    }
    return nb_allocated_event;
}

static inline void pmsis_event_pop(struct pmsis_event_kernel *event_kernel,
        struct pmsis_event **event)
{
    struct pmsis_event_scheduler *sched = event_kernel->scheduler;
    if(!sched->first)
    {
        *event = NULL;
        // if sched->first was null, and semaphore is taken twice, then no event
        // available, and no push is ongoing, so we'd better sleep
        pmsis_mutex_take(&event_kernel->event_sched_mutex);
        return;
    }
    // if we're here, cluster (or anything else) can't be pushing events

    // Critical section, cluster & FC might both try to be here
    // Cluster will have to go through IRQ, so locking irq is the easiest synchro
    int irq = pmsis_event_lock_cl_to_fc(event_kernel);
    *event = sched->first;
    sched->first = sched->first->next;
    if(sched->first == NULL)
    {// if we're at the end, reset last also
        sched->last=NULL;
    }
    pmsis_event_unlock_cl_to_fc(event_kernel, irq);
}

/** Might be called from cluster FC code, or ISR **/
int pmsis_event_push(struct pmsis_event_kernel_wrap *wrap, pi_task_t *task)
{
    struct pmsis_event_kernel *event_kernel = pmsis_event_wrap_get_kernel(wrap);
    // TODO: add cluster-fc synchro here
    //int irq = pmsis_event_lock_cl_to_fc(event_kernel);
    // Critical section, cluster & FC might both be here
    struct pmsis_event *event;

    struct pmsis_event_scheduler* sched = pmsis_event_wrap_get_scheduler(wrap);

    event = pmsis_event_get(sched);
    if(!event)
    {
        printf("struct pmsis_eventask: no free event, failing gracefully\n");
        return -1;
    }

    event->fc_task = task;

    if(sched->last)
    {
        sched->last->next = event;
        sched->last = event;
    }
    else
    {
        sched->last = event;
        sched->first = event;
    }

    // release spinlock or reactivate irqs
    //pmsis_event_unlock_cl_to_fc(event_kernel, irq);
    // signal event kernel that a task is available
    pmsis_mutex_release(&event_kernel->event_sched_mutex);
    return 0;
}

/**
 * Prepare the event kernel structure and task
 * In particular, create inner private structure
 * And setup synchronization mutexes
 */
int pmsis_event_kernel_init(struct pmsis_event_kernel_wrap **wrap,
        void (*event_kernel_entry)(void*))
{
    *wrap = malloc(sizeof(struct pmsis_event_kernel));
    struct pmsis_event_kernel_wrap *event_kernel_wrap = *wrap;
    if(!event_kernel_wrap)
    {
        return -1;
    }

    // allocate private struct and priv pointer to it
    struct pmsis_event_kernel *priv = malloc(sizeof(struct pmsis_event_kernel));
    event_kernel_wrap->priv = priv;
    if(!priv)
    {
        return -1;
    }

    struct pmsis_event_scheduler *sched = malloc(sizeof(struct pmsis_event_scheduler));
    if(!sched)
    {
        return -1;
    }

    sched->first=NULL;
    sched->first_free=NULL;
    sched->last=NULL;

    sched->allocated_event_nb=0;
    sched->max_event_nb=0;

    pmsis_event_wrap_set_scheduler(event_kernel_wrap, sched);

    // finally, initialize the mutex we'll use
    if(pmsis_mutex_init(&priv->event_sched_mutex))
    {
        return -1;
    }
    pmsis_mutex_take(&priv->event_sched_mutex);
    // TODO: check name for native handle
    event_kernel_wrap->__os_native_task = pmsis_task_create(event_kernel_entry,
            event_kernel_wrap,
            "event_kernel",
            PMSIS_TASK_MAX_PRIORITY);

    if(!event_kernel_wrap->__os_native_task)
    {
        return -1;
    }
    priv->running = 1;
    return 0;
}

static void pmsis_event_kernel_exec_event(struct pmsis_event_kernel *kernel,
        struct pmsis_event *event)
{
    switch(event->fc_task->id)
    {
        case PI_TASK_CALLBACK_ID :
            {
                callback_t callback_func = (callback_t)event->fc_task->arg[0];
                callback_func((void*)event->fc_task->arg[1]);
                *((volatile uint8_t*)&(event->fc_task->done)) = 1;
            }
            break;
        default: // unimplemented id
            break;
    }
}

void pmsis_event_kernel_main(void *arg)
{
    struct pmsis_event_kernel_wrap *wrap = (struct pmsis_event_kernel_wrap*)arg;
    struct pmsis_event_kernel *event_kernel = pmsis_event_wrap_get_kernel(wrap);
    pmsis_event_t *event;
    while(1)
    {
        pmsis_event_pop(event_kernel,&event);
        if(event)
        {
            pmsis_event_kernel_exec_event(event_kernel,event);
            pmsis_event_release(event);
        }
    }
}

void pmsis_event_kernel_destroy(struct pmsis_event_kernel_wrap **wrap)
{
    struct pmsis_event_kernel *event_kernel = pmsis_event_wrap_get_kernel(*wrap);
    pmsis_task_suspend((*wrap)->__os_native_task);
}

struct pmsis_event_kernel_wrap *pmsis_event_get_default_scheduler(void)
{
    return default_sched;
}

void pmsis_event_set_default_scheduler(struct pmsis_event_kernel_wrap *wrap)
{
    default_sched = wrap;
}

void pmsis_event_destroy_default_scheduler(struct pmsis_event_kernel_wrap *wrap)
{
    pmsis_event_kernel_destroy(&wrap);
}



#endif
