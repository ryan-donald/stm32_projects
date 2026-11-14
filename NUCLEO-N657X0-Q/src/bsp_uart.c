#include "bsp.h"
#include "bsp_uart.h"

static volatile uint8_t uart1_rx_buf[RX_BUF_SIZE];   // USART1 rx ring buffer
static volatile uint8_t uart1_tx_buf[TX_BUF_SIZE];   // USART1 tx ring buffer

static const uart_config_t uart1_config = {
    .uart_registers = USART1,
    .irq = USART1_IRQn,
    .ker_clk = 64000000UL,             // hsi_div_ck
    .rx_buf = uart1_rx_buf,
    .tx_buf = uart1_tx_buf,
};

uart_t uart1 = {
    .config = &uart1_config,
    .rx_head = 0,                      // next slot the interrupt writes
    .rx_tail = 0,                      // next slot uart_getc reads
    .tx_head = 0,                      // next slot uart_putc writes
    .tx_tail = 0,                      // next slot the interrupt sends
};

void uart1_init(uint32_t baud)
{
    // enable clocks for GPIOE and USART1
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOEEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    // register read to make sure the clock is running
    (void)RCC->APB2ENR;

    // USART1 clock = hsi_div_ck, clear USART1SEL and set it to 0b110
    RCC->CCIPR13 = (RCC->CCIPR13 & ~RCC_CCIPR13_USART1SEL)
                 | RCC_CCIPR13_USART1SEL_2 | RCC_CCIPR13_USART1SEL_1;

    // PE5 (TX), PE6 (RX) in alternate function mode, AF7 = USART1
    // clear MODE5/MODE6 bits and set each to 0b10 (AF)
    GPIOE->MODER = (GPIOE->MODER & ~(GPIO_MODER_MODE5 | GPIO_MODER_MODE6))
                 | GPIO_MODER_MODE5_1 | GPIO_MODER_MODE6_1;

    // clear AFSEL5/AFSEL6 bits and set each to 0b0111 (AF7)
    GPIOE->AFR[0] = (GPIOE->AFR[0] & ~(GPIO_AFRL_AFSEL5 | GPIO_AFRL_AFSEL6))
                  | (7 << GPIO_AFRL_AFSEL5_Pos) | (7 << GPIO_AFRL_AFSEL6_Pos);

    // clear PUPD6 bits and set them to 0b01 (pull-up)
    GPIOE->PUPDR = (GPIOE->PUPDR & ~GPIO_PUPDR_PUPD6) | GPIO_PUPDR_PUPD6_0;

    uart_init(&uart1, baud);
}

void USART1_IRQHandler(void)
{
    uart_irq(&uart1);
}
