#ifndef __SOC_CTRL_H__
#define __SOC_CTRL_H__

/*!
 * @addtogroup SOC_CTRL_Peripheral_Access_Layer SOC_CTRL Peripheral Access Layer
 * @{
 */

/** SOC_CTRL - Registers Layout Typedef */
typedef struct {
  __IO  uint32_t INFO;                /**< SOC_CTRL INFO register, offset: 0x00 */
  __IO  uint32_t _reserved0[2];       /**< reserved, offset: 0x04 */
  __IO  uint32_t CLUSTER_ISO;         /**< SOC_CTRL Cluster Isolate register, offset: 0x0C */
  __IO  uint32_t _reserved1[23];      /**< reserved, offset: 0x10 */
  __IO  uint32_t CLUSTER_BUSY;        /**< SOC_CTRL Busy register, offset: 0x6C */
  __IO  uint32_t CLUSTER_BYPASS;      /**< SOC_CTRL Cluster PMU bypass register, offset: 0x70 */
  __IO  uint32_t JTAG;                /**< SOC_CTRL Jtag register, offset: 0x74 */
  __IO  uint32_t L2_SLEEP;            /**< SOC_CTRL L2 memory sleep register, offset: 0x78 */
  __IO  uint32_t SLEEP_CTRL;          /**< SOC_CTRL Slepp control register, offset: 0x7C */
  __IO  uint32_t CLKDIV0;             /**< SOC_CTRL Slepp control register, offset: 0x80 */
  __IO  uint32_t CLKDIV1;             /**< SOC_CTRL Slepp control register, offset: 0x84 */
  __IO  uint32_t CLKDIV2;             /**< SOC_CTRL Slepp control register, offset: 0x88 */
  __IO  uint32_t CLKDIV3;             /**< SOC_CTRL Slepp control register, offset: 0x8C */
  __IO  uint32_t CLKDIV4;             /**< SOC_CTRL Slepp control register, offset: 0x90 */
  __IO  uint32_t _reserved2[3];       /**< reserved, offset: 0x94 */

  __IO  uint32_t CORE_STATUS;         /**< SOC_CTRL Slepp control register, offset: 0xA0 */
  __IO  uint32_t CORE_STATUS_EOC;     /**< SOC_CTRL Slepp control register, offset: 0xC0 */

} SOC_CTRL_Type, soc_ctrl_t;

/* ----------------------------------------------------------------------------
   -- SOC_CTRL Register Masks
   ---------------------------------------------------------------------------- */

/*!
 * @addtogroup SOC_CTRL_Register_Masks SOC_CTRL Register Masks
 * @{
 */
/*! @name CLUSTER_BYPASS - SOC_CTRL information register */
#define SOC_CTRL_CLUSTER_BYPASS_BYP_POW_MASK          (0x1U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_POW_SHIFT         (0U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_POW(x)            (((uint32_t)(((uint32_t)(x)) /* << SOC_CTRL_CLUSTER_BYPASS_BYP_POW_SHIFT*/)) & SOC_CTRL_CLUSTER_BYPASS_BYP_POW_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_BYP_CFG_MASK          (0x2U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_CFG_SHIFT         (1U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_CFG(x)            (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_BYP_CFG_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_BYP_CFG_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_POW_MASK              (0x8U)
#define SOC_CTRL_CLUSTER_BYPASS_POW_SHIFT             (3U)
#define SOC_CTRL_CLUSTER_BYPASS_POW(x)                (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_POW_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_POW_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_CUURENT_SET_MASK      (0x70U)
#define SOC_CTRL_CLUSTER_BYPASS_CUURENT_SET_SHIFT     (4U)
#define SOC_CTRL_CLUSTER_BYPASS_CUURENT_SET(x)        (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_CUURENT_SET_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_CUURENT_SET_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_DELAY_MASK            (0x180U)
#define SOC_CTRL_CLUSTER_BYPASS_DELAY_SHIFT           (7U)
#define SOC_CTRL_CLUSTER_BYPASS_DELAY(x)              (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_DELAY_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_DELAY_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_BYP_CLK_MASK          (0x200U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_CLK_SHIFT         (9U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_CLK(x)            (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_BYP_CLK_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_BYP_CLK_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_CLK_GATE_MASK         (0x400U)
#define SOC_CTRL_CLUSTER_BYPASS_CLK_GATE_SHIFT        (10U)
#define SOC_CTRL_CLUSTER_BYPASS_CLK_GATE(x)           (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_CLK_GATE_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_CLK_GATE_MASK)
#define READ_SOC_CTRL_CLUSTER_BYPASS_CLK_GATE(x)      (((uint32_t)(((uint32_t)(x)) & SOC_CTRL_CLUSTER_BYPASS_CLK_GATE_MASK)) >> SOC_CTRL_CLUSTER_BYPASS_CLK_GATE_SHIFT)

#define SOC_CTRL_CLUSTER_BYPASS_FLL_PWD_MASK          (0x800U)
#define SOC_CTRL_CLUSTER_BYPASS_FLL_PWD_SHIFT         (11U)
#define SOC_CTRL_CLUSTER_BYPASS_FLL_PWD(x)            (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_FLL_PWD_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_FLL_PWD_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_FLL_RET_MASK          (0x1000U)
#define SOC_CTRL_CLUSTER_BYPASS_FLL_RET_SHIFT         (12U)
#define SOC_CTRL_CLUSTER_BYPASS_FLL_RET(x)            (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_FLL_RET_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_FLL_RET_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_RESET_MASK            (0x2000U)
#define SOC_CTRL_CLUSTER_BYPASS_RESET_SHIFT           (13U)
#define SOC_CTRL_CLUSTER_BYPASS_RESET(x)              (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_RESET_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_RESET_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_BYP_ISO_MASK          (0x4000U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_ISO_SHIFT         (14U)
#define SOC_CTRL_CLUSTER_BYPASS_BYP_ISO(x)            (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_BYP_ISO_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_BYP_ISO_MASK)

#define SOC_CTRL_CLUSTER_BYPASS_PW_ISO_MASK           (0x8000U)
#define SOC_CTRL_CLUSTER_BYPASS_PW_ISO_SHIFT          (15U)
#define SOC_CTRL_CLUSTER_BYPASS_PW_ISO(x)             (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CLUSTER_BYPASS_PW_ISO_SHIFT)) & SOC_CTRL_CLUSTER_BYPASS_PW_ISO_MASK)
#define READ_SOC_CTRL_CLUSTER_BYPASS_PW_ISO(x)        (((uint32_t)(((uint32_t)(x)) & SOC_CTRL_CLUSTER_BYPASS_PW_ISO_MASK)) >> SOC_CTRL_CLUSTER_BYPASS_PW_ISO_SHIFT)

/*! @name JTAG - SOC_CTRL jtag control register */
#define SOC_CTRL_JTAG_INT_SYNC_MASK          (0x1U)
#define SOC_CTRL_JTAG_INT_SYNC_SHIFT         (0U)
#define SOC_CTRL_JTAG_INT_SYNC(x)            (((uint32_t)(((uint32_t)(x)) /* << SOC_CTRL_JTAG_INT_SYNC_SHIFT*/)) & SOC_CTRL_JTAG_INT_SYNC_MASK)
#define READ_SOC_CTRL_JTAG_INT_SYNC(x)       (((uint32_t)(((uint32_t)(x)) & SOC_CTRL_JTAG_INT_SYNC_MASK)) /* >> SOC_CTRL_JTAG_INT_SYNC_SHIFT*/)

#define SOC_CTRL_JTAG_INT_BT_MD_MASK         (0xEU)
#define SOC_CTRL_JTAG_INT_BT_MD_SHIFT        (1U)
#define SOC_CTRL_JTAG_INT_BT_MD(x)           (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_JTAG_INT_BT_MD_SHIFT)) & SOC_CTRL_JTAG_INT_BT_MD_MASK)
#define READ_SOC_CTRL_JTAG_INT_BT_MD(x)      (((uint32_t)(((uint32_t)(x)) & SOC_CTRL_JTAG_INT_BT_MD_MASK)) >> SOC_CTRL_JTAG_INT_BT_MD_SHIFT)

#define SOC_CTRL_JTAG_EXT_SYNC_MASK          (0x100U)
#define SOC_CTRL_JTAG_EXT_SYNC_SHIFT         (8U)
#define SOC_CTRL_JTAG_EXT_SYNC(x)            (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_JTAG_EXT_SYNC_SHIFT)) & SOC_CTRL_JTAG_EXT_SYNC_MASK)
#define READ_SOC_CTRL_JTAG_EXT_SYNC(x)       (((uint32_t)(((uint32_t)(x)) & SOC_CTRL_JTAG_EXT_SYNC_MASK)) >> SOC_CTRL_JTAG_EXT_SYNC_SHIFT)

#define SOC_CTRL_JTAG_EXT_BT_MD_MASK         (0xE00U)
#define SOC_CTRL_JTAG_EXT_BT_MD_SHIFT        (9U)
#define SOC_CTRL_JTAG_EXT_BT_MD(x)           (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_JTAG_EXT_BT_MD_SHIFT)) & SOC_CTRL_JTAG_EXT_BT_MD_MASK)
#define READ_SOC_CTRL_JTAG_EXT_BT_MD(x)      (((uint32_t)(((uint32_t)(x)) & SOC_CTRL_JTAG_EXT_BT_MD_MASK)) >> SOC_CTRL_JTAG_EXT_BT_MD_SHIFT)

/*! @name STATUS - SOC_CTRL status register */
#define SOC_CTRL_CORE_STATUS_EOC_MASK                 (0x80000000U)
#define SOC_CTRL_CORE_STATUS_EOC_SHIFT                (31U)
#define SOC_CTRL_CORE_STATUS_EOC(x)                   (((uint32_t)(((uint32_t)(x)) << SOC_CTRL_CORE_STATUS_EOC_SHIFT)) & SOC_CTRL_CORE_STATUS_EOC_MASK)

/*!
 * @}
 */ /* end of group SOC_CTRL_Register_Masks */



#endif
