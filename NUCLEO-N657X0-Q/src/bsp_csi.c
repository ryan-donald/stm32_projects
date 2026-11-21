#include "bsp.h"
#include "bsp_csi.h"
#include "vl53l9_frame.h"

#define CSI_BUF_SIZE ((VL53L9_CSI_FRAME_MAX + CSI_BUF_ALIGN - 1) & ~(CSI_BUF_ALIGN - 1))   // largest frame rounded up to the alignment
#define RIF_MASTER_DCMIPP 9            // RIMC_ATTRx index of DCMIPP
#define RIF_PERIPH_DCMIPP 29           // RISC_SECCFGR2/PRIVCFGR2 bit of DCMIPP

static uint8_t csi_buf[CSI_BUF_COUNT][CSI_BUF_SIZE] __attribute__((aligned(CSI_BUF_ALIGN)));   // frames written by DCMIPP

static const csi_config_t csi_config = {
    .phy_band = CSI_PHY_BAND_1000,     // sensor sends 1 Gbit/s on one lane
    .osc_freq_target = 460,
    .data_type = VL53L9_CSI_DATA_TYPE,
    .buf_size = CSI_BUF_SIZE,
    .buf = { csi_buf[0], csi_buf[1], csi_buf[2] },
};

csi_t csi = {
    .config = &csi_config,
};

// sets up clocks, resets, security and the interrupt for CSI and DCMIPP, needs clock_init
void csi_board_init(void)
{
    // IC17 = PLL1 / 4 = 300 MHz for DCMIPP, IC18 = PLL1 / 80 = 15 MHz for the D-PHY
    RCC->IC17CFGR = (0 << RCC_IC17CFGR_IC17SEL_Pos) | ((4 - 1) << RCC_IC17CFGR_IC17INT_Pos);
    RCC->IC18CFGR = (0 << RCC_IC18CFGR_IC18SEL_Pos) | ((80 - 1) << RCC_IC18CFGR_IC18INT_Pos);
    RCC->DIVENSR = RCC_DIVENSR_IC17ENS | RCC_DIVENSR_IC18ENS;

    // DCMIPP kernel clock = IC17, clear DCMIPPSEL and set it to 0b10
    RCC->CCIPR1 = (RCC->CCIPR1 & ~RCC_CCIPR1_DCMIPPSEL) | RCC_CCIPR1_DCMIPPSEL_1;

    // enable clocks for DCMIPP, CSI and RIFSC
    RCC->APB5ENR |= RCC_APB5ENR_DCMIPPEN | RCC_APB5ENR_CSIEN;
    RCC->AHB3ENR |= RCC_AHB3ENR_RIFSCEN;
    (void)RCC->APB5ENR;

    // pulse reset on DCMIPP and CSI
    RCC->APB5RSTSR = RCC_APB5RSTSR_DCMIPPRSTS | RCC_APB5RSTSR_CSIRSTS;
    RCC->APB5RSTCR = RCC_APB5RSTCR_DCMIPPRSTC | RCC_APB5RSTCR_CSIRSTC;

    // let DCMIPP write to secure memory, where the frame buffers are
    RIFSC->RIMC_ATTRx[RIF_MASTER_DCMIPP] = (RIFSC->RIMC_ATTRx[RIF_MASTER_DCMIPP]
                                            & ~(RIFSC_RIMC_ATTRx_MCID | RIFSC_RIMC_ATTRx_MSEC | RIFSC_RIMC_ATTRx_MPRIV))
                                         | (1 << RIFSC_RIMC_ATTRx_MCID_Pos) | RIFSC_RIMC_ATTRx_MSEC
                                         | RIFSC_RIMC_ATTRx_MPRIV;

    // make the DCMIPP registers secure and privileged
    RIFSC->RISC_SECCFGRx[2] |= 1UL << RIF_PERIPH_DCMIPP;
    RIFSC->RISC_PRIVCFGRx[2] |= 1UL << RIF_PERIPH_DCMIPP;

    csi_init(&csi);

    NVIC_EnableIRQ(DCMIPP_IRQn);
}

void DCMIPP_IRQHandler(void)
{
    csi_irq(&csi);
}
