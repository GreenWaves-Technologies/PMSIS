#ifndef __PMSIS_H__
#define __PMSIS_H__

/* Include main HAL and transitive headers. */
// pmsis_hal will include targets, which should not be used outside of hals
#include "pmsis_hal.h"
#include "pmsis_intrsc.h"
/* Include cluster side of pmsis. */
#include "pmsis_cluster/cl_pmsis.h"

// include hals:
/** Core hals **/
#include "pmsis_itc.h"
/** end core hals **/

/** soc hals **/
#include "pmsis_soc_eu.h"
#include "pmsis_fll.h"
/** end soc hals **/

/** UDMA hals **/
#include "udma_core.h"
#include "udma_ctrl.h"
#include "udma_sdio.h"
#include "udma_spim.h"
//#include "udma_i2c.h"
/** End UDMA hals **/

/* Performance Counter */
#include "pmsis_perf.h"
/* End Performance Counter */

#define malloc (pmsis_malloc)
#define free (pmsis_malloc_free)

#endif
