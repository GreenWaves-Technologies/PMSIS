#if 1
#include "string.h"

#include "pmsis.h"
#include "fc_to_cl_delegate.h"
#include "cl_synchronisation.h"
#include "cl_team.h"
#include "pmsis_soc_eu.h"
#include "pmsis_eu.h"
#include "core_utils.h"

#include "pmsis_l1_malloc.h"
// we need to explicitely map all shared structures to L2
#include "pmsis_l2_malloc.h"
#include "pmsis_malloc.h"
#include "pmsis_task.h"

#include "cl_to_fc_delegate.h"
#include "event_kernel.h"

#define NB_CLUSTER 1
#ifdef __GAP8__
#define CLUSTER_NB_CORES 8
#else
#define CLUSTER_NB_CORES 9
#endif
#define CORE_STACK_SIZE 0x400

#define PRINTF(...) ((void)0)

extern spinlock_t cluster_printf_spinlock;

// ----------------------------------
// Need to be rewritten with OS agnostic symbols
extern char  __l1_preload_start;
extern char  __l1_preload_start_inL2;
extern char  __l1_preload_size;

#if defined(__GAP8__)
extern char  __l1FcShared_start;
extern char  __l1FcShared_size;
#elif defined(__VEGA__)
extern char  __l1Shared_start;
extern char  __l1Shared_size;
#endif

extern char  __heapsram_start;
extern char  __heapsram_size;
// ----------------------------------
#include "pmsis_backend_native_pmu_api.h"
#if 1

struct cluster_driver_data *__per_cluster_data[NB_CLUSTER] = {NULL};

static inline void __cluster_start(struct pi_device *device);
static inline void __cluster_start_async(struct pi_device *device, pi_task_t *async_task);
static inline int __cluster_stop(struct pi_device *device);
static inline int __cluster_stop_async(struct pi_device *device, pi_task_t *async_task);
static inline int __cluster_has_task(struct cluster_driver_data *data);

static void __cluster_free_device_event_func(void *arg);

static inline void pi_push_cluster_task(struct cluster_driver_data *data, struct pi_cluster_task *task);
static inline void cl_pop_cluster_task(struct cluster_driver_data *data);

// L2 data as L1 cluster might disappear, and is shared between cluster and FC

void cl_task_finish(void);

// accessible through second level ptr or through ioctl
static const struct cluster_driver_api __spec_cluster_api = {
    .send_task = pi_cluster_send_task_to_cl,
    .send_task_async = pi_cluster_send_task_to_cl_async,
    .wait_free = pi_cluster_wait_free,
    .wait_free_async = pi_cluster_wait_free_async
};

static const struct pi_device_api __cluster_api = {
    .open = pi_cluster_open,
    .close = pi_cluster_close,
    .open_async = pi_cluster_open_async,
    .close_async = pi_cluster_close_async,
    .ioctl = pi_cluster_ioctl,
    .ioctl_async = pi_cluster_ioctl_async
};

static const struct cluster_driver_conf __cluster_default_conf = {
    .device_type = PI_DEVICE_CLUSTER_TYPE,
    // by default, us cluster 0
    .id = 0,
    // default for heap if no address provided by user.
    .heap_start = (void*)0x10000004,
    .heap_size = 0x400
};

void cl_cluster_exec_loop()
{
    /* Cluster Icache Enable*/
    SCBC->ICACHE_ENABLE = 0xFFFFFFFF;

    /* Initialization for the task dispatch loop */
    hal_eu_evt_mask_set((1 << EU_DISPATCH_EVENT)
            | (1 << EU_MUTEX_EVENT)
            | (1 << EU_HW_BARRIER_EVENT)
            | (1 << EU_LOOP_EVENT));

    asm volatile ("add    s1,  x0,  %0" :: "r" (CORE_EU_DISPATCH_DEMUX_BASE));
    asm volatile ("add    s2,  x0,  %0" :: "r" (CORE_EU_BARRIER_DEMUX_BASE));
    asm volatile ("add    s3,  x0,  %0" :: "r" (CORE_EU_CORE_DEMUX_BASE));

    if(__native_core_id())
    {
        while(1) {
            asm volatile (
                    "1:\n\t"
                    "p.elw  t0,  0(s1)         \n\t"
                    "p.elw  a0,  0(s1)         \n\t"
                    "andi   t1,  t0,   1       \n\t"
                    "bne    t1,  zero, 2f      \n\t"
                    "jalr   t0                 \n\t"
                    "p.elw  t0,  0x1c(s2)         \n\t"
                    "j      3f                 \n"
                    "2:\n\t"
                    "p.bclr t0, t0, 0,0        \n\t"
                    "jalr   t0                 \n"
                    "3:"
                    );
        } // slaveloop
    } else {
        /*
         * Core 0 will wait for tasks from FC side
         */

        __enable_irq();

        asm volatile("add   s4,  x0,  %0\n\t"
                :
                : "r"(__per_cluster_data)
                );
        while(1)
        {
            // MasterLoop
            asm volatile (
                    "1:\n\t"
                    // EU_EVT_MaskWaitAndClr(1 << FC_NOTIFY_CLUSTER_EVENT);
                    "p.bset  t1, x0, 0, %0 \n\t"
                    "sw      t1, 0x8(s3)   \n\t"
                    "p.elw   t0, 0x3C(s3)  \n\t"
                    "sw      t1, 0x4(s3)   \n\t"
                    // check whether driver is init, sleep otherwise
                    "lw      s5, (s4)      \n\t"
                    "beqz    s5, 4f        \n\t"
                    // task check
                    "lw      s0, (s5)      \n\t"
                    "beqz    s0, 4f        \n\t"

                    // stack init
                    "lw      a0, 8(s0)     \n\t"
                    "lw      a1, 12(s0)    \n\t"
                    "add     sp, a0, a1    \n\t"
                    "mv      a0, s0        \n\t"
                    "jal     cl_stack_init \n\t"

                    "lw      a1, (s0)       \n\t"
                    "lw      a0, 4(s0)      \n\t"
                    "jalr    a1             \n\t"
                    "jal     cl_task_finish \n\t"
                    "4:"
                    :
                    : "I" (FC_NOTIFY_CLUSTER_EVENT)
                    );
        }
    } // masterloop
}

/*!
 * @brief All cores set stack.
 *
 * @param  stacksPtr     Overall stack pointer for all cores.
 * @param  coreStackSize Each core's stack size.
 * @note .
 */
static void cl_set_core_stack(void *arg)
{
    struct pi_cluster_task *task = arg;
    if(__native_core_id())
    {
        // slaves
        uint32_t core_stack_ptr = (uint32_t) ((__native_core_id() + 1UL) * task->stack_size + (uint32_t)task->stacks);

        /* Update sp */
        asm ("add  sp, x0, %0" :: "r" (core_stack_ptr) );
    }
    else
    {
        // master
        /* Cluster calls FC */
#if defined(__GAP8__)
        EU_FC_EVT_TrigSet(CLUSTER_NOTIFY_FC_EVENT, 0);
#elif defined(__VEGA__)
        FC_ITC->STATUS_SET = (1 << CLUSTER_NOTIFY_FC_EVENT);
#endif
    }
}

/*!
 * @brief All cores do task init.
 * Master core 0 do stacks initilization.
 * @param.
 * @note .
  */
void cl_stack_init(struct pi_cluster_task *task)
{
    PRINTF("cl_stack_init: going in\n");
    cl_team_fork(task->nb_cores, cl_set_core_stack, (void *)task);
    PRINTF("cl_stack_init: going out\n");
}


void cl_task_finish(void)
{
    int id = __native_cluster_id();

    // TODO: send callback if it exists
    // -----
    struct cluster_driver_data *data = __per_cluster_data[id];
    struct pi_cluster_task *task = data->task_first;

    PRINTF("cl_task_finish: task=%p\n",task);

    if(task->completion_callback)
    {
        cl_send_task_to_fc(task->completion_callback);
    }
    // -----
    // clean up finished cluster task
    cl_pop_cluster_task(data);
    PRINTF("cl_task_finish: data=%p\n",data);

    // Notify FC that current task is done
#if defined(__GAP8__)
    hal_eu_fc_evt_trig_set(CLUSTER_NOTIFY_FC_EVENT, 0);
#elif defined(__VEGA__)
    FC_ITC->STATUS_SET = (1 << CLUSTER_NOTIFY_FC_EVENT);
#endif
}

/**
 * \brief open the cluster described by device with given conf
 */
int pi_cluster_open(struct pi_device *device)
{
    void *conf = device->config;
    PRINTF("OPEN_W_CONF: device=%p\n",device);
    device->api = (struct pi_device_api*)pmsis_l2_malloc(sizeof(struct pi_device_api));
    memcpy(device->api, &__cluster_api, sizeof(struct pi_device_api));
    if(conf == NULL)
    {
        // In this case, heap will be at the first address of L1 cluster
        device->config = pmsis_l2_malloc(sizeof(struct cluster_driver_conf));
        PRINTF("device->config=%p\n",device->config);
        memcpy(device->config, &__cluster_default_conf, sizeof(struct cluster_driver_conf));
    }

    struct cluster_driver_conf *_conf = (struct cluster_driver_conf *)device->config;
    device->data = __per_cluster_data[_conf->id];
    PRINTF("OPEN---precheck: device->data=%p\n",device->data);
    // if device data has not yet been populated
    if(device->data == NULL)
    {
        device->data = (void*) pmsis_l2_malloc(sizeof(struct cluster_driver_data));
        PRINTF("OPEN--post-check: device->data=%p\n",device->data);
        struct cluster_driver_data *_data = (struct cluster_driver_data *)device->data;
        memset(_data, 0, sizeof(struct cluster_driver_data));
        pmsis_mutex_init(&_data->task_mutex);
        pmsis_mutex_init(&_data->powerstate_mutex);
        // fill the per cluster data ptr
        __per_cluster_data[_conf->id] = _data;
        PRINTF("per cluster data[%lx]=%lx\n",_conf->id,__per_cluster_data[_conf->id]);

        // create an event task to manage cluster driver
        struct pmsis_event_kernel_wrap *wrap = NULL;
        pmsis_event_kernel_init(&wrap, pmsis_event_kernel_main);
        pmsis_event_alloc(wrap, 10);
        _data->event_kernel = wrap;
    }
    PRINTF("near start: device->config=%p\n",device->config);
    PRINTF("near start: _conf:=%p, device->config->heap_start=%p\n",_conf, _conf->heap_start);

    _conf->heap_start = (void*)&__heapsram_start;
    _conf->heap_size = (uint32_t)&__heapsram_size;

    __cluster_start(device);
    mc_fc_delegate_init(NULL);
    return 0;
}

int pi_cluster_open_async(struct pi_device *device,
        pi_task_t *async_task)
{
    void *conf = device->config;

    // if device has not yet been populated
    if(device->data == NULL)
    {
        device->api = (struct pi_device_api*)pmsis_l2_malloc(sizeof(struct pi_device_api));
        memcpy(device->api, &__cluster_api, sizeof(struct pi_device_api));
        if(conf == NULL)
        {
            // In this case, heap will be at the first address of L1 cluster
            device->config = pmsis_l2_malloc(sizeof(struct cluster_driver_conf));
            memcpy(device->config, &__cluster_default_conf, sizeof(struct cluster_driver_conf));
        }
        device->data = (void*) pmsis_l2_malloc(sizeof(struct cluster_driver_data));
        struct cluster_driver_data *_data = (struct cluster_driver_data *)device->data;
        memcpy(device->config, &__cluster_default_conf, sizeof(struct cluster_driver_conf));
    }
    __cluster_start_async(device->data, async_task);
    return 0;
}

int pi_cluster_close(struct pi_device *device)
{
    int ret = __cluster_stop(device);
    struct cluster_driver_data *_data = (struct cluster_driver_data *)device->data;
    pmsis_event_kernel_destroy(&_data->event_kernel);
    if(!ret)
    {   // if no task has an active handle on device
        // --> clean up data as everything in L1 is going to be lost
        PRINTF("Cluster clean ups\n");
        struct cluster_driver_conf *_conf = (struct cluster_driver_conf *)device->config;
        __per_cluster_data[_conf->id] = NULL; // pointer makes no sense anymore, reset it
        // free all structures
        free(device->data);
        free(device->api->specific_api);
        free(device->api);
        free(device->config);
        free(device);
    }
    return ret;
}

int pi_cluster_close_async(struct pi_device *device, pi_task_t *async_task)
{
#if 0
    __cluster_stop_async(device, async_task);

    pi_task_t free_task;
    free_task.id = FC_TASK_CALLBACK_ID;
    free_task.arg[0] = (uint32_t)__cluster_free_device_event_func;
    free_task.arg[1] = (uint32_t)device;
    pi_event_append(async_task, free_task);
#endif
    return 0;
}

uint32_t pi_cluster_ioctl(struct pi_device *device, uint32_t func_id, void *arg)
{
    struct pi_device_api *api = device->api;
    struct cluster_driver_api *cluster_api = api->specific_api;
    uint32_t ret = 0;
    switch(func_id)
    {
        case SEND_TASK_ID:
            {
                struct pi_cluster_task *cl_task = (struct pi_cluster_task*)arg;
                cluster_api->send_task(device,cl_task);
            }
            break;
        case WAIT_FREE_ID:
            {
                cluster_api->wait_free(device);
            }
            break;
        case OPEN_ID:
            {
                device->api->open(device);
            }
            break;
        case CLOSE_ID:
            {
                device->api->close(device);
            }
            break;
        default:
            break;
    }

    return ret;
}

uint32_t pi_cluster_ioctl_async(struct pi_device *device, uint32_t func_id,
        void *arg, pi_task_t *async_task)
{
    struct pi_device_api *api = device->api;
    struct cluster_driver_api *cluster_api = api->specific_api;
    uint32_t ret = 0;
    switch(func_id)
    {
        case SEND_TASK_ASYNC_ID:
            {
                struct pi_cluster_task *cl_task = (struct pi_cluster_task*)arg;
                cluster_api->send_task_async(device,cl_task,async_task);
            }
            break;
        case WAIT_FREE_ASYNC_ID:
            {
                cluster_api->wait_free_async(device,async_task);
            }
            break;
        case OPEN_ASYNC_ID:
            {
                api->open_async(device,async_task);
            }
            break;
        case CLOSE_ASYNC_ID:
            {
                api->close_async(device,async_task);
            }
            break;
        default:
            break;
    }
    return ret;
}


static inline void __cluster_free_device_event_func(void *arg)
{
    struct pi_device *device = (struct pi_device *)arg;
    // free all structures
    //free(device->data, sizeof(struct cluster_driver_data));
    //free(device->api->specific_api, sizeof(struct cluster_driver_api));
    //free(device->api, sizeof(struct pi_device_api));
    //free(device, sizeof(struct pi_device));
}

static inline void __cluster_start(struct pi_device *device)
{
    PRINTF("__cluster_start: device=%p\n",device);
    struct cluster_driver_data *data = (struct cluster_driver_data *)device->data;
    struct cluster_driver_conf *conf = (struct cluster_driver_conf *)device->config;
    pmsis_mutex_take(&data->powerstate_mutex);
    // --- critical zone start ---
    if(!data->cluster_is_on)
    {
        // call pmu through os for now, TODO: add the driver in PMSIS
        __os_native_pmu_cluster_poweron();
        PRINTF("poweron is done\n");
        for (int i = 0; i < CLUSTER_CORES_NUM; i++)
        {
#if defined(__GAP8__)
            SCB->BOOT_ADDR[i] = 0x1C000100;
#elif defined(__VEGA__)
            SCB->BOOT_ADDR[i] = 0x1C008100;
#endif
        }
        SCB->FETCH_EN = 0xFFFFFFFF;

        /* In case the chip does not support L1 preloading, the initial L1 data are in L2, we need to copy them to L1 */
        memcpy((char *)GAP_CLUSTER_TINY_DATA(0, (int)&__l1_preload_start), &__l1_preload_start_inL2, (size_t)&__l1_preload_size);

        /* Copy the FC / clusters shared data as the linker can only put it in one section (the cluster one) */
#if defined(__GAP8__)
        memcpy((char *)GAP_CLUSTER_TINY_DATA(0, (int)&__l1FcShared_start), &__l1FcShared_start, (size_t)&__l1FcShared_size);
#elif defined(__VEGA__)
        memcpy((char *)GAP_CLUSTER_TINY_DATA(0, (int)&__l1Shared_start), &__l1Shared_start, (size_t)&__l1Shared_size);
#endif

        PRINTF("conf:%p, heap_start:%p, heap_size:%lx\n",conf, conf->heap_start, conf->heap_size);
        pmsis_l1_malloc_init(conf->heap_start,conf->heap_size);

        int32_t *lock_addr = pmsis_l1_malloc(sizeof(int32_t));
        cl_sync_init_spinlock(&data->fifo_access, lock_addr);
        cl_sync_init_spinlock(&cluster_printf_spinlock, pmsis_l1_malloc(sizeof(uint32_t)));
        pmsis_event_lock_cl_to_fc_init(data->event_kernel);
    }
    data->cluster_is_on++;
    pmsis_mutex_release(&data->powerstate_mutex);
    PRINTF("started cluster\n");
    PRINTF("data ptr: %p\n",data);
}

static inline int __cluster_stop(struct pi_device *device)
{
    struct cluster_driver_data *data = (struct cluster_driver_data *)device->data;
    pmsis_mutex_take(&data->powerstate_mutex);
    // decrement usage counter
    data->cluster_is_on--;

    // if no other tasks are using the cluster, begin poweroff procedure
    if(!data->cluster_is_on)
    {
        // wait for potential remaining tasks
        //__cluster_wait_termination(device);
        __os_native_pmu_cluster_poweroff();
    }
    pmsis_mutex_release(&data->powerstate_mutex);
    return data->cluster_is_on;
}

static inline int __cluster_has_task(struct cluster_driver_data *data)
{
    volatile int has_task = 0;
    cl_sync_spinlock_take(&data->fifo_access);
    struct pi_cluster_task *task = data->task_first;
    has_task = !(data->task_first == NULL);
    cl_sync_spinlock_release(&data->fifo_access);
    return has_task;
}

static inline void pi_push_cluster_task(struct cluster_driver_data *data, struct pi_cluster_task *task)
{
    // critical section for cluster and fc
    // Use spinlock as it is pretty much the only reliable synchronisation we have
    cl_sync_spinlock_take(&data->fifo_access);
    task->next=NULL;

    PRINTF("task=%p, data->task_first=%lx\n",task, data->task_first);
    if(data->task_last == NULL)
    {
        data->task_first = task;
        data->task_last = task;
    }
    else
    {
        data->task_last->next = task;
    }
    PRINTF("task=%p, data->task_first=%lx\n",task, data->task_first);
    cl_sync_spinlock_release(&data->fifo_access);
}

static inline void cl_pop_cluster_task(struct cluster_driver_data *data)
{
    // critical section for cluster and fc
    // Uses spinlock as it is pretty much the only reliable synchronisation we have
    cl_sync_spinlock_take(&data->fifo_access);
    struct pi_cluster_task *task = data->task_first;
    data->task_first = task->next;
    if(data->task_first == NULL)
    {
        data->task_last = NULL;
    }
    cl_sync_spinlock_release(&data->fifo_access);
    if(task->stack_allocated)
    {
        // put everything back as it was before
        pmsis_l1_malloc_free(task->stacks, task->stack_size);
        task->stacks = NULL;
        task->stack_size = 0;
        task->stack_allocated = 0;
    }
}

static inline void __cluster_start_async(struct pi_device *device, pi_task_t *async_task)
{
    struct cluster_driver_data *cluster_data = (struct cluster_driver_data *)device->data;
}

static inline int __cluster_stop_async(struct pi_device *device, pi_task_t *async_task)
{
    struct cluster_driver_data *cluster_data = (struct cluster_driver_data *)device->data;
    return 0;
}

static inline int __pi_send_task_to_cl(struct pi_device *device, struct pi_cluster_task *task)
{
    struct cluster_driver_data *data = (struct cluster_driver_data *)device->data;
    pmsis_mutex_take(&data->task_mutex);

    if(!data->cluster_is_on)
    {// cluster is not on, can not reliably use L1 ram
        pmsis_mutex_release(&data->task_mutex);
        return -1;
    }

    /* If Cluster has not finished previous task, wait */

    task->nb_cores = task->nb_cores ? task->nb_cores : CLUSTER_NB_CORES;

    PRINTF("task->nb_cores:%lx\n",task->nb_cores);

    // SWAR bit count --> replace by hw count if exists
    uint32_t nb_cores = task->nb_cores;

    task->stack_size  = task->stack_size ? task->stack_size : CORE_STACK_SIZE;
    if(!task->stacks)
    {
        task->stacks = pmsis_l1_malloc(nb_cores * task->stack_size);
        PRINTF("task->stacks=%p\n",task->stacks);
        task->stack_allocated = 1; // to remember freeing memory if allocated by us
    }
    else
    {
        task->stack_allocated = 0;
    }

    pi_push_cluster_task(data, task);

    // Wake cluster up
    EU_CLUSTER_EVT_TrigSet(FC_NOTIFY_CLUSTER_EVENT, 0);
    pmsis_mutex_release(&data->task_mutex);
    return 0;
}

int pi_cluster_send_task_to_cl(struct pi_device *device, struct pi_cluster_task *task)
{
    while(__pi_send_task_to_cl(device,task))
    {
        __os_native_yield();
    }
    return 0;
}

int pi_cluster_send_task_to_cl_async(struct pi_device *device, struct pi_cluster_task *task, pi_task_t *fc_task)
{
    return 0;
}

void pi_cluster_wait_free(struct pi_device *device)
{
    /* If Cluster has not finished previous task, wait */
    while(__cluster_has_task((struct cluster_driver_data *)device->data))
    {
        //__os_native_yield();
    }
}

void pi_cluster_wait_free_async(struct pi_device *device, pi_task_t *async_task)
{

}

uint8_t pi_cluster_is_on(void)
{
    for(int i=0; i<NB_CLUSTER; i++)
    {
        if(__per_cluster_data[i] && __per_cluster_data[i]->cluster_is_on)
        {
            return 1;
        }
    }
    return 0;
}


void pi_cluster_conf_init(struct cluster_driver_conf *conf)
{
    conf->id = 0;
}


#endif
#endif
