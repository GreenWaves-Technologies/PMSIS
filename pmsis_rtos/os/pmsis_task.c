#include "pmsis_os.h"

pi_task_t *pi_task_callback(pi_task_t *callback_task, void (*func)(void *), void *arg)
{
    callback_task->id = PI_TASK_CALLBACK_ID;
    callback_task->arg[0] = (uintptr_t)func;
    callback_task->arg[1] = (uintptr_t)arg;
    callback_task->done = 0;
    pmsis_mutex_init(&(callback_task->wait_on));
    // lock the mutex so that task may be descheduled while waiting on it
    pmsis_mutex_take(&(callback_task->wait_on));
    return callback_task;
}

pi_task_t *pi_task_block(pi_task_t *callback_task)
{
    callback_task->id = PI_TASK_NONE_ID;
    callback_task->done = 0;
    pmsis_mutex_init(&(callback_task->wait_on));
    // lock the mutex so that task may be descheduled while waiting on it
    pmsis_mutex_take(&(callback_task->wait_on));
    return callback_task;
}

void pi_task_release(pi_task_t *task)
{
    // if the mutex is only virtual (e.g. wait on soc event)
    task->done = 1;
    // if the sched support semaphore/mutexes
    pmsis_mutex_release(&(task->wait_on));
}

void pi_task_wait_on(pi_task_t *task)
{
    // if the mutex is only virtual (e.g. wait on soc event)
    while(!hal_read8(&task->done))
    {
        // FIXME: workaround for gcc bug
        hal_compiler_barrier();
        // if the underlying scheduler support it, deschedule the task
        pmsis_mutex_take(&task->wait_on);
        hal_compiler_barrier();
    }
    // now that the wait is done, deinit the mutex as it is purely internal
    pmsis_mutex_deinit(&task->wait_on);
}

