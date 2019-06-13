/*
 *              VEGA PMSIS_HAL File (VEGA)
 *  This file contains only the Hardware Abstraction Layer of VEGA microprocessor.
 *  Aims is to unify the low level SW of chips (PULP and CMSIS), and to give programs
 *  a standard access to the hardware resources.
 *
 *      Example: read/write SPIM0 reg CMD_CFG:
 *
 *  hal_write32(&SPIM0->CMD_CFG, 0xAA);
 *  hal_read32(&SPIM0->CMD_CFG);
 *
 */

#ifndef __PMSIS_HAL_H__
#define __PMSIS_HAL_H__

#ifdef __GAP8__
#define TARGET_CHIP     GAP8
#define PI_CLUSTER_NB_CORES 8
#elif defined(__VEGA__)
#define TARGET_CHIP     VEGA
#define PI_CLUSTER_NB_CORES 9
#endif

#define  __CHIP_INC(x)  #x
#define  _CHIP_INC(x)   __CHIP_INC(targets/x/pmsis_periph.h)
#define  CHIP_INC(x)    _CHIP_INC(x)

#include CHIP_INC(TARGET_CHIP)

#include "core_utils.h"

/**
 *  Compiler barriers: essential to make sure that read/writes are all flushed
 *  by compiler before next instructions
 **/

// make the compiler believe we have potential side effects
#define hal_compiler_barrier()\
{\
    __asm__ __volatile__ ("" : : : "memory");\
}

#define hal_write32( addr, value ) \
{ \
    hal_compiler_barrier();\
    *(volatile uint32_t*)(volatile uint32_t)(addr) = (uint32_t)value; \
    hal_compiler_barrier();\
}

#define hal_write8( addr, value ) \
{ \
    hal_compiler_barrier();\
    *(volatile uint8_t*)(volatile uint8_t)(addr) = (uint8_t)value; \
    hal_compiler_barrier();\
}

#define hal_or32( addr, value ) \
{ \
    hal_compiler_barrier();\
    *(volatile uint32_t*)(volatile uint32_t)(addr) |= (uint32_t)value; \
    hal_compiler_barrier();\
}


#define hal_and32( addr, value ) \
{ \
    hal_compiler_barrier();\
    *((volatile uint32_t*)(volatile uint32_t)addr) &= value; \
    hal_compiler_barrier();\
}

static inline uint32_t hal_read32(volatile void *addr) 
{ 
    hal_compiler_barrier();
    uint32_t ret = (*((volatile uint32_t*)(volatile uint32_t)addr));
    hal_compiler_barrier();
    return ret;
}

static inline uint8_t hal_read8(volatile void *addr) 
{ 
    hal_compiler_barrier();
    uint8_t ret = (*((volatile uint8_t*)(volatile uint32_t)addr));
    hal_compiler_barrier();
    return ret;
}

/*!
 * @addtogroup event_unit
 * @{
 */

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/** @name ITC management
 *  The following function can be used for selecting which events can wake-up the core, put the core the sleep, clear events and so on.
 */
/**@{*/


/*
 *  Compatibility Section
 *  For ensuring our SW could run in both pulp-os and mbed-os, we keep these
 *  part until we have our whole PMSIS done.
 */

// PULP OS
#define pulp_read32        hal_read32
#define pulp_write32       hal_write32

#define hal_itc_wait_for_event_noirq    hal_itc_wait_for_event_no_irq
#define hal_itc_wait_for_event          hal_itc_wait_for_event_no_irq

#endif



