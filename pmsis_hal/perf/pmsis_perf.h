#ifndef __PERFORMANCE_H__
#define __PERFORMANCE_H__

typedef struct perf_s
{
    uint32_t perf_mask;                   /*!< Mask of the Performance Counter. */
    uint32_t perf_count[PCER_EVENTS_NUM]; /*!< Array to hold Performance Counter values. */
} perf_t;

/*!
 * @brief Set Performance mask.
 * @param perf_mask Mask of the peformance counters to enable.
 */
static inline void pmsis_perf_init(uint32_t perf_mask)
{
    __PCER_Set(perf_mask);
}

/*!
 * @brief Set all Performance registers to a given value.
 * @param value     Value to set to all registers.
 */
static inline void pmsis_perf_set(uint32_t value)
{
    __PCCR31_Set(value);
}

/*!
 * @brief Reset all Performance registers to 0.
 */
static inline void pmsis_perf_reset()
{
    pmsis_perf_set(0);
}

/*!
 * @brief Enable Performance counter(s).
 */
static inline void pmsis_perf_enable()
{
    __PCMR_Set(PCMR_GLBEN_Msk | PCMR_SATU_Msk);
}

/*!
 * @brief Start Performance counter(s).
 *
 * This function starts Performance counter(s), according to a given mask of events.
 *
 * @param perf      Structure used to hold mask and values.
 */
static inline void pmsis_perf_start(perf_t *perf)
{
    pmsis_perf_init(perf->perf_mask);
    pmsis_perf_reset();
    pmsis_perf_enable();
}

/*!
 * @brief Get Performance counter(s)' value(s).
 *
 * This function saves Performance counter(s)' value(s).
 *
 * @param perf      Structure used to hold mask and values.
 */
static inline void pmsis_perf_get(perf_t *perf)
{
    uint32_t perf_mask = perf->perf_mask;
    do
    {
        uint32_t event = __FL1(perf_mask);
        /* Remove current event from mask. */
        perf_mask &= ~(1 << event);
        perf->perf_count[event] += __PCCRs_Get(event);
    } while (perf_mask);
}

/*!
 * @brief Stop Performance counter(s).
 *
 * This function stops Performance counter(s) and saves counters' value(s).
 *
 * @param perf      Structure used to hold mask and values.
 */
static inline void pmsis_perf_stop(perf_t *perf)
{
    pmsis_perf_get(perf);
    /* Stop & disable Performance counters. */
    __PCMR_Set(0);
}

#endif  /* __PERFORMANCE_H__ */
