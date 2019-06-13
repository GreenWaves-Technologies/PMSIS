/*
 * Copyright (C) 2018 ETH Zurich, University of Bologna and GreenWaves Technologies
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cl_to_fc_delegate.h"
#include "event_kernel.h"
#include "pmsis_it.h"
#include "pmsis_eu.h"

pi_task_t *g_task;

extern struct cluster_driver_data *__per_cluster_data[];
void cluster_cl_event_handler(void);

HANDLER_WRAPPER_LIGHT(cluster_cl_event_handler);

/**
 * @ingroup groupCluster
 */

/**
 * @defgroup FC_Delegate
 * Delegation of task to FC by cluster API
 */

/**
 * @addtogroup FC_Delegate
 * @{
 */

/**@{*/

void cluster_cl_event_handler(void)
{
    struct cluster_driver_data *data = __per_cluster_data[0];
    pmsis_event_push(data->event_kernel, g_task);
    g_task= (pi_task_t *)0xdeadbeef;
}


/** \brief send an opaque task structure for FC
 *
 * FC will execute task according to the opaque structure
 * (callback, driver access...)
 * \param        opaque argument for the fc
 */
void cl_send_task_to_fc(pi_task_t *task)
{
    while(g_task != (pi_task_t *)0xdeadbeef)
    {
        asm volatile ("nop");
    }
    g_task = task;
    hal_eu_fc_evt_trig_set(CLUSTER_NOTIFY_FC_IRQN, 0);
}

void mc_fc_delegate_init(void *arg)
{
    g_task=(pi_task_t *)0xdeadbeef;
    /* Activate interrupt handler for FC when cluster want to push a task to FC */
    NVIC_SetVector(CLUSTER_NOTIFY_FC_IRQN, (uint32_t)__handler_wrapper_light_cluster_cl_event_handler);
    NVIC_EnableIRQ(CLUSTER_NOTIFY_FC_IRQN);
    return;
}

