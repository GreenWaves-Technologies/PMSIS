#include "rtos/pmsis_driver_core_api/pmsis_driver_core_api.h"
#include "pmsis_os.h"
#include "cl_pmsis.h"

static inline pi_device_e get_pi_device_type(void *conf);

// extract pi_device_type
static inline pi_device_e get_pi_device_type(void *conf)
{
    pi_device_e *device_ptr = (pi_device_e*)conf;
    return *device_ptr;
}

void pi_open_from_conf(struct pi_device *device, void *conf)
{
    pi_device_e device_type = get_pi_device_type(conf);
    switch(device_type)
    {
        case PI_DEVICE_CLUSTER_TYPE:
            device->config = conf;
            pi_cluster_open(device);
            break;
        default:
            // TODO: add debug warning here
            break;
    }
}
