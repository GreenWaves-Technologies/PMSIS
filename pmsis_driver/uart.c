/*
 * Copyright (c) 2019, GreenWaves Technologies, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of GreenWaves Technologies, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "udma_uart.h"
#include "uart.h"
#include "event_kernel.h"
#include "pmsis_task.h"
#include "pmsis_l2_malloc.h"
#include "pmsis_fc_event.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define UART_TX_BUFFER_SIZE     16
#define UART_DEFAULT_PRE_ALLOC_EVT 5

#define NB_UART 1
uint8_t uart_byte_buffer[1];
/* Indicate whether uart is initialed */
uint8_t uart_is_init = 0;

/* UART transfer state. */
enum uart_tansfer_states_e
{
    UART_TX_IDLE,          /* TX idle. */
    UART_TX_BUSY,          /* TX busy. */
    UART_RX_IDLE,          /* RX idle. */
    UART_RX_BUSY,          /* RX busy. */
    UART_RX_PARITY_ERROR   /* Rx parity error */
};

/*! @brief UART transfer structure. */
#define  uart_transfer_t udma_req_info_t

/*! @brief UART request structure. */
#define  uart_req_t udma_req_t

/* Forward declaration of the handle typedef. */
typedef struct uart_handle uart_handle_t;

/*! @brief UART transfer callback function. */
typedef void (*uart_transfer_callback_t)(uart_handle_t *handle);


/*******************************************************************************
 * Variables
 ******************************************************************************/
#if (defined(UART))
#define UART_HANDLE_ARRAY_SIZE 1
#else /* UART */
#error No UART instance.
#endif /* UART */

/*******************************************************************************
 * Driver data
 *****************************************************************************/

struct uart_drv_task
{
    pi_task_t *pi_task;
    struct uart_drv_task *next;
};

struct uart_driver_data
{
    struct uart_drv_task *uart_fifo_head;
    struct uart_drv_task *uart_fifo_tail;
    // pre allocated to avoid needless malloc
    struct uart_drv_task *uart_fifo_free_list_head;
    uint8_t uart_id;
};

struct uart_driver_data *__global_uart_drv_data[NB_UART];

/*******************************************************************************
 * Prototypes & inlines
 ******************************************************************************/

static inline int __pi_uart_write(struct uart_driver_data *data, void *buffer, uint32_t size, pi_task_t *callback);

void __pi_uart_write_callback(void *arg);

static inline int __pi_uart_read(struct uart_driver_data *data, void *buffer, uint32_t size, pi_task_t *callback);

void  __pi_uart_read_callback(void *arg);

void uart_handler(void *arg);

static inline struct uart_drv_task *__uart_fifo_get_free(struct uart_driver_data *data)
{
    struct uart_drv_task *ret_task;
    ret_task = data->uart_fifo_free_list_head;
    if(data->uart_fifo_free_list_head)
    {
        data->uart_fifo_free_list_head = data->uart_fifo_free_list_head->next;
    }
    return ret_task;
}

static inline void __uart_fifo_push_free(struct uart_driver_data *data,
        struct uart_drv_task *free_task)
{
    free_task->next = data->uart_fifo_free_list_head;
    data->uart_fifo_free_list_head = free_task;
}

static inline int __uart_drv_fifo_not_empty(struct uart_driver_data *data)
{
    return (!!data->uart_fifo_head);
}

// Has to be synchronized with irq_disabled since irq handler might pop at the same time
static inline void __uart_drv_fifo_enqueue(struct uart_driver_data *data,
        pi_task_t *pi_task)
{
    int irq = __disable_irq();
    if(data->uart_fifo_head)
    {
        // tail insert
        data->uart_fifo_tail->next      = __uart_fifo_get_free(data);
        data->uart_fifo_tail            = data->uart_fifo_tail->next;
        data->uart_fifo_tail->pi_task   = pi_task;
        data->uart_fifo_tail->next      = NULL;
    }
    else
    {
        // Initialize the list
        data->uart_fifo_head          = __uart_fifo_get_free(data);
        data->uart_fifo_head->next    = NULL;
        data->uart_fifo_head->pi_task = pi_task;
        // set the base tail
        data->uart_fifo_tail          = data->uart_fifo_head;
    }
    __restore_irq(irq);
}

static inline pi_task_t *__uart_drv_fifo_pop(struct uart_driver_data *data)
{
    int irq = __disable_irq();
    pi_task_t *ret_task = NULL;
    if(data->uart_fifo_head)
    {
        ret_task = data->uart_fifo_head->pi_task;
        struct uart_drv_task *to_free = data->uart_fifo_head;
        hal_compiler_barrier();
        data->uart_fifo_head = data->uart_fifo_head->next;
        if(data->uart_fifo_head == NULL)
        {
            data->uart_fifo_tail = NULL;
        }
        __uart_fifo_push_free(data, to_free);
    }
    __restore_irq(irq);
    return ret_task;
}

__attribute__((section(".text"))) __noinline
void uart_handler(void *arg)
{
#ifdef __GAP8__
    uint32_t uart_id = 0;
#else
    uint32_t uart_id = (uint32_t)arg;
#endif

    pi_task_t *task = __uart_drv_fifo_pop(__global_uart_drv_data[uart_id]);

    if(task->id == PI_TASK_NONE_ID)
    {
        pi_task_release(task);
    }
    else
    {
        if(task->id == PI_TASK_CALLBACK_ID)
        {
            pmsis_event_push(pmsis_event_get_default_scheduler(), task);
        }
    }

}

/***********
 * Inner functions
 ***********/

struct uart_callback_args {
    struct uart_driver_data *data;
    void *buffer;
    uint32_t size;
    pi_task_t *callback;
};

static inline pi_task_t *__uart_prepare_write_callback(struct uart_driver_data *data,
        void *buffer, uint32_t size, pi_task_t *callback)
{
    // TODO: use slab alloc? FIXME: free at the end!!
    pi_task_t *write_callback = pmsis_l2_malloc(sizeof(pi_task_t));
    struct uart_callback_args *callback_args = pmsis_l2_malloc(sizeof(struct uart_callback_args));

    // prepare callback args
    callback_args->data     = data;
    callback_args->buffer   = buffer;
    callback_args->size     = size;
    callback_args->callback = callback;

    // prepare callback tasks
    write_callback->id = PI_TASK_CALLBACK_ID;
    write_callback->arg[0] = (uintptr_t) __pi_uart_write_callback;
    write_callback->arg[1] = (uintptr_t) callback_args;

    return write_callback;
}

void  __pi_uart_write_callback(void *arg)
{
    struct uart_callback_args *cb_args = (struct uart_callback_args *)arg;

    // have to block irq for a short time here as driver is irq driven
    __uart_drv_fifo_enqueue(cb_args->data, cb_args->callback);
    // Enqueue transfer directly since no one is using uart
    udma_uart_enqueue_transfer(cb_args->data->uart_id, cb_args->buffer, cb_args->size, TX_CHANNEL);

    pmsis_l2_malloc_free(cb_args, sizeof(struct uart_callback_args));
}

static inline pi_task_t *__uart_prepare_read_callback(struct uart_driver_data *data,
        void *buffer, uint32_t size, pi_task_t *callback)
{
    // TODO: use slab alloc?
    pi_task_t *write_callback = pmsis_l2_malloc(sizeof(pi_task_t));
    struct uart_callback_args *callback_args = pmsis_l2_malloc(sizeof(struct uart_callback_args));

    // prepare callback args
    callback_args->data   = data;
    callback_args->buffer   = buffer;
    callback_args->size     = size;
    callback_args->callback = callback;

    // prepare callback tasks
    write_callback->id = PI_TASK_CALLBACK_ID;
    write_callback->arg[0] = (uintptr_t)__pi_uart_read_callback;
    write_callback->arg[1] = (uintptr_t)callback_args;

    return write_callback;
}


void  __pi_uart_read_callback(void *arg)
{
    struct uart_callback_args *cb_args = (struct uart_callback_args *)arg;

    // have to block irq for a short time here as driver is irq driven
    __uart_drv_fifo_enqueue(cb_args->data, cb_args->callback);
    // Enqueue transfer directly since no one is using uart
    udma_uart_enqueue_transfer(cb_args->data->uart_id, cb_args->buffer, cb_args->size, RX_CHANNEL);

    pmsis_l2_malloc_free(cb_args, sizeof(struct uart_callback_args));
}

// Inner function with actual write logic
int __pi_uart_write(struct uart_driver_data *data, void *buffer, uint32_t size, pi_task_t *callback)
{
    // Due to udma restriction, we need to use an L2 address,
    // Since the stack is probably in FC tcdm, we have to either ensure users gave
    // us an L2 pointer or alloc ourselves
    if(((uintptr_t)buffer & 0xFFF00000) != 0x1C000000)
    {
        return -1;
    }

    // have to block irq for a short time here as driver is irq driven
    int irq = __disable_irq();
    if(__uart_drv_fifo_not_empty(data))
    {
        __uart_drv_fifo_enqueue(data,__uart_prepare_write_callback(data, buffer, size, callback));
    }
    else
    {
        __uart_drv_fifo_enqueue(data, callback);
        // Enqueue transfer directly since no one is using uart
        udma_uart_enqueue_transfer(data->uart_id, buffer, size, TX_CHANNEL);
    }
    __restore_irq(irq);
    return 0;
}

static inline int __pi_uart_read(struct uart_driver_data *data, void *buffer, uint32_t size, pi_task_t *callback)
{ 
    // Due to udma restriction, we need to use an L2 address,
    // Since the stack is probably in FC tcdm, we have to either ensure users gave
    // us an L2 pointer or alloc ourselves
    if(((uintptr_t)buffer & 0xFFF00000) != 0x1C000000)
    {
        return -1;
    }   

    // have to block irq for a short time here as driver is irq driven
    int irq = __disable_irq();
    if(__uart_drv_fifo_not_empty(data))
    {
        __uart_drv_fifo_enqueue(data, __uart_prepare_read_callback(data, buffer, size, callback));
    }
    else
    {
        __uart_drv_fifo_enqueue(data, callback);
        udma_uart_enqueue_transfer(data->uart_id, buffer, size, RX_CHANNEL);
    }
    __restore_irq(irq);
    return 0;
}

/*******************************************************************************
 ********************** PMSIS FC functions implem ******************************
 ******************************************************************************/


/*!
 * @brief Gets the default configuration structure.
 *
 * This function initializes the UART configuration structure to a default value. The default
 * values are as follows.
 *   pi_uart_conf->baudRate_Bps = 115200U;
 *   pi_uart_conf->parity_mode = UART_PARITY_DISABLED;
 *   pi_uart_conf->stop_bit_count = UART_ONE_STOP_BIT;
 *   pi_uart_conf->enable_tx = false;
 *   pi_uart_conf->enable_rx = false;
 *
 * @param config Pointer to configuration structure.
 */
void pi_uart_conf_init(struct pi_uart_conf *conf)
{
    memset(conf, 0, sizeof(struct pi_uart_conf));
    conf->uart_id = 0;
    conf->baudrate_bps = 115200U;
    conf->parity_mode  = UART_PARITY_DISABLED;

    conf->stop_bit_count = UART_ONE_STOP_BIT;

    conf->enable_tx = false;
    conf->enable_rx = false;

    conf->nb_event = UART_DEFAULT_PRE_ALLOC_EVT;
}

int pi_uart_open(pi_device_t *device)
{
    struct pi_uart_conf *conf = device->config;

    struct uart_driver_data *data = pmsis_l2_malloc(sizeof(struct uart_driver_data));
    memset(data,0,sizeof(struct uart_driver_data));

    for(volatile int i = 0; i<conf->nb_event; i++)
    {
        __uart_fifo_push_free(data, pmsis_l2_malloc(sizeof(struct uart_drv_task)));
    }
 
    data->uart_id = conf->uart_id;
    device->data = data;

    // prepare handler here
    // ------------------------

    pi_fc_event_handler_set(UDMA_EVENT_UART_RX, uart_handler);
    pi_fc_event_handler_set(UDMA_EVENT_UART_TX, uart_handler);
    hal_soc_eu_set_fc_mask(UDMA_EVENT_UART_RX);
    hal_soc_eu_set_fc_mask(UDMA_EVENT_UART_TX);

    // ------------------------

    // init physical uart
    udma_uart_init(conf->uart_id, conf);
    __global_uart_drv_data[conf->uart_id] = data;
    return 0;
}

int pi_uart_write(pi_device_t *device, void *buffer, uint32_t size)
{
    int ret = 0;
    pi_task_t task_block;

    pi_task_block(&task_block);

    if(__pi_uart_write(device->data, buffer, size, &task_block))
    {
        return -1;
    }

    pi_task_wait_on(&task_block);
    return ret;
}

int pi_uart_write_async(pi_device_t *device, void *buffer, uint32_t size,
        pi_task_t *callback)
{
    return __pi_uart_write(device->data, buffer, size, callback);
}

// FIXME/TODO: add some kind of bufferization?
int pi_uart_write_byte(pi_device_t *device, uint8_t *byte)
{
    int ret = pi_uart_write(device, byte, 1);
    return ret;
}

int pi_uart_write_byte_async(pi_device_t *device, uint8_t *byte, pi_task_t *callback)
{
    return pi_uart_write_async(device, byte, 1, callback);
}

int pi_uart_read(pi_device_t *device, void *buffer, uint32_t size)
{
    // Due to udma restriction, we need to use an L2 address,
    // Since the stack is probably in FC tcdm, we have to either ensure users gave
    // us an L2 pointer or alloc ourselves
    if(((uintptr_t)buffer & 0xFFF00000) != 0x1C000000)
    {
        return -1;
    }

    pi_task_t task_block;
    //memset(&task_block, 0, sizeof(pi_task_t));
    pi_task_block(&task_block);

    __pi_uart_read(device->data, buffer, size, &task_block);

    pi_task_wait_on(&task_block);
    return 0;
}

int pi_uart_read_async(pi_device_t *device, void *buffer, uint32_t size,
        pi_task_t *callback)
{
    struct uart_driver_data *data = device->data;
    __uart_drv_fifo_enqueue(data, __uart_prepare_read_callback(data, buffer, size, callback));
    return 0;
}

void pi_uart_close(pi_device_t *device)
{
    // TODO
}
