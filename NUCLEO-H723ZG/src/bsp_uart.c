#include "bsp.h"
#include "bsp_uart.h"

static volatile uint8_t uart3_rx_buf[RX_BUF_SIZE];   // USART3 rx ring buffer
static volatile uint8_t uart3_tx_buf[TX_BUF_SIZE];   // USART3 tx ring buffer

static const uart_config_t uart3_config = {
    .uart_registers = USART3,
    .irq = USART3_IRQn,
    .ker_clk = 64000000UL,             // hsi_ker
    .rx_buf = uart3_rx_buf,
    .tx_buf = uart3_tx_buf,
};

uart_t uart3 = {
    .config = &uart3_config,
    .rx_head = 0,                      // next slot the interrupt writes
    .rx_tail = 0,                      // next slot uart_getc reads
    .tx_head = 0,                      // next slot uart_putc writes
    .tx_tail = 0,                      // next slot the interrupt sends
};

void uart3_init(uint32_t baud)
{
    // enable clocks for GPIOD and USART3
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIODEN;
    RCC->APB1LENR |= RCC_APB1LENR_USART3EN;

    // register read to make sure the clock is running
    (void)RCC->APB1LENR;

    // USART3 clock = hsi_ker, clear USART28SEL and set it to 0b011
    RCC->D2CCIP2R = (RCC->D2CCIP2R & ~RCC_D2CCIP2R_USART28SEL)
                  | RCC_D2CCIP2R_USART28SEL_1 | RCC_D2CCIP2R_USART28SEL_0;

    // PD8, PD9 in alternate function mode, AF7 = USART3
    // clear MODE8/MODE9 bits and set each to 0b10 (AF)
    GPIOD->MODER = (GPIOD->MODER & ~(GPIO_MODER_MODE8 | GPIO_MODER_MODE9))
                 | GPIO_MODER_MODE8_1 | GPIO_MODER_MODE9_1;

    // clear AFSEL8/AFSEL9 bits and set each to 0b0111 (AF7)
    GPIOD->AFR[1] = (GPIOD->AFR[1] & ~(GPIO_AFRH_AFSEL8 | GPIO_AFRH_AFSEL9))
                  | (7 << GPIO_AFRH_AFSEL8_Pos) | (7 << GPIO_AFRH_AFSEL9_Pos);

    // clear PUPD9 bits and set them to 0b01 (pull-up)
    GPIOD->PUPDR = (GPIOD->PUPDR & ~GPIO_PUPDR_PUPD9) | GPIO_PUPDR_PUPD9_0;

    uart_init(&uart3, baud);
}

void USART3_IRQHandler(void)
{
    uart_irq(&uart3);
}
