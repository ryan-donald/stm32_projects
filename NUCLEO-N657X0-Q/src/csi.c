#include "csi.h"
#include "delay.h"

#define CSI_CFG_CLK_RANGE   0x28    // PFCR CCFR, D-PHY config clock range
#define CSI_DT_BPP8         2       // 8 bits per pixel
#define CSI_DATA_LANE0      1       // lane mapping, physical lane 0
#define CSI_DATA_LANE1      2       // lane mapping, physical lane 1
#define CSI_TIMEOUT_MS      100

// writes a D-PHY register through its test interface
static void phy_write(uint32_t reg, uint32_t val)
{
    // send the upper register address byte as 0
    CSI->PTCR1 = CSI_PTCR1_TWM;
    CSI->PTCR0 = CSI_PTCR0_TCKEN;
    CSI->PTCR0 = 0;
    CSI->PTCR1 = 0;
    CSI->PTCR0 = CSI_PTCR0_TCKEN;

    // send the lower register address byte
    CSI->PTCR0 = 0;
    CSI->PTCR1 = CSI_PTCR1_TWM;
    CSI->PTCR0 = CSI_PTCR0_TCKEN;
    CSI->PTCR1 = CSI_PTCR1_TWM | (reg & 0xFF);
    CSI->PTCR0 = 0;
    CSI->PTCR1 = 0;

    // send the data byte
    CSI->PTCR1 = val & 0xFF;
    CSI->PTCR0 = CSI_PTCR0_TCKEN;
    CSI->PTCR0 = 0;
}

// sets up the CSI-2 receiver with one data lane and DCMIPP pipe0
void csi_init(csi_t *csi)
{
    const csi_config_t *config = csi->config;

    // use 1 data lane on physical lane 0, then enable CSI
    CSI->CR = 0;
    CSI->LMCFGR = (1 << CSI_LMCFGR_LANENB_Pos) | (CSI_DATA_LANE0 << CSI_LMCFGR_DL0MAP_Pos)
                | (CSI_DATA_LANE1 << CSI_LMCFGR_DL1MAP_Pos);
    CSI->CR = CSI_CR_CSIEN;

    // hold the D-PHY in reset with all lanes off to configure
    CSI->PRCR = 0;
    CSI->PCR = 0;

    // pulse testclk to reset the test interface
    CSI->PTCR0 = CSI_PTCR0_TCKEN;
    delay_ms(1);
    CSI->PTCR0 = 0;

    // set the config clock range and frequency range
    CSI->PFCR = (CSI_CFG_CLK_RANGE << CSI_PFCR_CCFR_Pos) | (config->phy_band << CSI_PFCR_HSFR_Pos);

    // D-PHY deskew and DLL target frequency
    phy_write(0x08, 0x38);
    phy_write(0xE4, 0x11);
    phy_write(0xE3, config->osc_freq_target >> 8);
    phy_write(0xE2, config->osc_freq_target & 0xFF);

    // enable the clock lane and data lane 0, power up and release reset
    CSI->PFCR = (CSI_CFG_CLK_RANGE << CSI_PFCR_CCFR_Pos) | (config->phy_band << CSI_PFCR_HSFR_Pos)
              | CSI_PFCR_DLD;
    CSI->PCR = CSI_PCR_DL0EN | CSI_PCR_CLEN | CSI_PCR_NPWRDOWN;
    CSI->PRCR = CSI_PRCR_PEN;
    CSI->PMCR = 0;

    // receive every data type on virtual channel 0 as 8-bit data
    CSI->VC0CFGR1 = (CSI_DT_BPP8 << CSI_VC0CFGR1_CDTFT_Pos) | CSI_VC0CFGR1_ALLDT;

    // pipe0 captures only the configured data type from virtual channel 0, every frame
    DCMIPP->P0FSCR = (0 << DCMIPP_P0FSCR_DTMODE_Pos) | (config->data_type << DCMIPP_P0FSCR_DTIDA_Pos)
                   | (0 << DCMIPP_P0FSCR_VC_Pos);
    DCMIPP->PRCR &= ~DCMIPP_PRCR_ENABLE;
    DCMIPP->CMCR |= DCMIPP_CMCR_INSEL;
    DCMIPP->P0FCTCR = 0;

    csi->buffers.capture = 0;
    csi->buffers.waiting = 1;
    csi->buffers.in_use = 2;
    csi->new_frame = 0;
    csi->frame_count = 0;
    csi->errors = 0;
}

// starts virtual channel 0 and pipe0, call before the sensor starts sending
int csi_start(csi_t *csi)
{
    const csi_config_t *config = csi->config;

    // start the virtual channel and wait until it is active
    CSI->CR |= CSI_CR_VC0START;
    uint32_t t = 0;
    while (!(CSI->SR0 & CSI_SR0_VC0STATEF))
    {
        if (t++ >= CSI_TIMEOUT_MS)
        {
            return -1;
        }
        delay_ms(1);
    }

    // capture one frame at a time, the interrupt starts the next one
    DCMIPP->P0FCTCR = DCMIPP_P0FCTCR_CPTMODE;
    DCMIPP->P0PPM0AR1 = (uint32_t)config->buf[csi->buffers.capture];

    // stop writing at the end of the buffer, the limit is in 32-bit words
    DCMIPP->P0DCLMTR = DCMIPP_P0DCLMTR_ENABLE | ((config->buf_size / 4) << DCMIPP_P0DCLMTR_LIMIT_Pos);

    // clear the flags and enable the frame and error interrupts
    DCMIPP->CMFCR = DCMIPP_CMFCR_CP0FRAMEF | DCMIPP_CMFCR_CP0OVRF | DCMIPP_CMFCR_CP0LIMITF
                  | DCMIPP_CMFCR_CATXERRF;
    DCMIPP->CMIER |= DCMIPP_CMIER_P0FRAMEIE | DCMIPP_CMIER_P0OVRIE | DCMIPP_CMIER_P0LIMITIE
                   | DCMIPP_CMIER_ATXERRIE;

    // enable the pipe and request the next frame
    DCMIPP->P0FSCR |= DCMIPP_P0FSCR_PIPEN;
    DCMIPP->P0FCTCR |= DCMIPP_P0FCTCR_CPTREQ;
    return 0;
}

// returns the newest frame
const uint8_t *csi_get_frame(csi_t *csi)
{

    if (csi->new_frame)
    {
        NVIC_DisableIRQ(DCMIPP_IRQn);

        uint8_t temp = csi->buffers.in_use;
        csi->buffers.in_use = csi->buffers.waiting;
        csi->buffers.waiting = temp;

        csi->new_frame = 0;

        NVIC_EnableIRQ(DCMIPP_IRQn);

        return csi->config->buf[csi->buffers.in_use];
    }
    else
    {
        return NULL;
    }

}

// frame done or error interrupt
void csi_irq(csi_t *csi)
{
    uint32_t flags = DCMIPP->CMSR2;

    // count overrun, limit and bus errors
    if (flags & (DCMIPP_CMSR2_P0OVRF | DCMIPP_CMSR2_P0LIMITF | DCMIPP_CMSR2_ATXERRF))
    {
        DCMIPP->CMFCR = DCMIPP_CMFCR_CP0OVRF | DCMIPP_CMFCR_CP0LIMITF | DCMIPP_CMFCR_CATXERRF;
        csi->errors++;
    }

    if (flags & DCMIPP_CMSR2_P0FRAMEF)
    {
        DCMIPP->CMFCR = DCMIPP_CMFCR_CP0FRAMEF;
        csi->frame_count++;

        uint8_t temp = csi->buffers.waiting;
        csi->buffers.waiting = csi->buffers.capture;
        csi->buffers.capture = temp;

        csi->new_frame = 1;

        // capture the next frame into the capture buffer
        DCMIPP->P0PPM0AR1 = (uint32_t)csi->config->buf[csi->buffers.capture];
        DCMIPP->P0FCTCR |= DCMIPP_P0FCTCR_CPTREQ;
    }
}
