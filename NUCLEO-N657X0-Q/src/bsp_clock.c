#include "bsp.h"
#include "bsp_clock.h"

// clock tree:
//   PLL1 = HSI 64 MHz / 4 * 75 = 1200 MHz
//   CPU  = IC1  = PLL1 / 2 = 600 MHz
//   bus  = IC2  = PLL1 / 3 = 400 MHz (IC6 / 4, IC11 / 3), AHB = bus / 2, APBs = AHB
//   timers = bus = 400 MHz
void clock_init(void)
{
    // skip PLL1 setup if it is already running
    if (!(RCC->SR & RCC_SR_PLL1RDY))
    {
        // spread spectrum off, no bypass, HSI source, M = 4, N = 75
        RCC->PLL1CFGR3 |= RCC_PLL1CFGR3_PLL1MODSSDIS;
        RCC->PLL1CFGR1 = (RCC->PLL1CFGR1 & ~(RCC_PLL1CFGR1_PLL1BYP | RCC_PLL1CFGR1_PLL1SEL
                                             | RCC_PLL1CFGR1_PLL1DIVM | RCC_PLL1CFGR1_PLL1DIVN))
                       | (4 << RCC_PLL1CFGR1_PLL1DIVM_Pos) | (75 << RCC_PLL1CFGR1_PLL1DIVN_Pos);

        // post dividers 1 and 1, integer mode
        RCC->PLL1CFGR3 = (RCC->PLL1CFGR3 & ~(RCC_PLL1CFGR3_PLL1PDIV1 | RCC_PLL1CFGR3_PLL1PDIV2
                                             | RCC_PLL1CFGR3_PLL1MODDSEN))
                       | (1 << RCC_PLL1CFGR3_PLL1PDIV1_Pos) | (1 << RCC_PLL1CFGR3_PLL1PDIV2_Pos);
        RCC->PLL1CFGR2 &= ~RCC_PLL1CFGR2_PLL1DIVNFRAC;
        RCC->PLL1CFGR3 |= RCC_PLL1CFGR3_PLL1MODSSRST | RCC_PLL1CFGR3_PLL1PDIVEN;

        // switch on and wait for lock
        RCC->CSR = RCC_CSR_PLL1ONS;
        while (!(RCC->SR & RCC_SR_PLL1RDY)) { };
    }

    // AHB = bus / 2 before the bus speeds up, APB dividers stay at 1
    RCC->CFGR2 = (RCC->CFGR2 & ~RCC_CFGR2_HPRE) | RCC_CFGR2_HPRE_0;

    // CPU on IC1 = PLL1 / 2
    RCC->IC1CFGR = (0 << RCC_IC1CFGR_IC1SEL_Pos) | ((2 - 1) << RCC_IC1CFGR_IC1INT_Pos);
    RCC->DIVENSR = RCC_DIVENSR_IC1ENS;
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_CPUSW) | RCC_CFGR1_CPUSW_1 | RCC_CFGR1_CPUSW_0;
    while ((RCC->CFGR1 & RCC_CFGR1_CPUSWS) != (RCC_CFGR1_CPUSWS_1 | RCC_CFGR1_CPUSWS_0)) { };

    // system bus on IC2 = PLL1 / 3, with IC6 = PLL1 / 4 and IC11 = PLL1 / 3
    RCC->IC2CFGR = (0 << RCC_IC2CFGR_IC2SEL_Pos) | ((3 - 1) << RCC_IC2CFGR_IC2INT_Pos);
    RCC->IC6CFGR = (0 << RCC_IC6CFGR_IC6SEL_Pos) | ((4 - 1) << RCC_IC6CFGR_IC6INT_Pos);
    RCC->IC11CFGR = (0 << RCC_IC11CFGR_IC11SEL_Pos) | ((3 - 1) << RCC_IC11CFGR_IC11INT_Pos);
    RCC->DIVENSR = RCC_DIVENSR_IC2ENS | RCC_DIVENSR_IC6ENS | RCC_DIVENSR_IC11ENS;
    RCC->CFGR1 = (RCC->CFGR1 & ~RCC_CFGR1_SYSSW) | RCC_CFGR1_SYSSW_1 | RCC_CFGR1_SYSSW_0;
    while ((RCC->CFGR1 & RCC_CFGR1_SYSSWS) != (RCC_CFGR1_SYSSWS_1 | RCC_CFGR1_SYSSWS_0)) { };

    // timer clock = bus clock
    RCC->CFGR2 &= ~RCC_CFGR2_TIMPRE;

    SystemCoreClockUpdate();
}
