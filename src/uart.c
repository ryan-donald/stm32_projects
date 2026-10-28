#include "stm32h7xx.h"
#include "uart.h"

#define UART_KER_CLK 64000000UL         // USART3 kernel clock (hsi_ker)
#define RX_BUF_SIZE 64                  // UART receive buffer, must be a power of 2

static volatile char rx_buf[RX_BUF_SIZE];   // UART rx ring buffer
static volatile uint32_t rx_head;           // next slot the interrupt writes
static volatile uint32_t rx_tail;           // next slot uart_getc reads

void uart_init(uint32_t baud)
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

    
    // disable USART to configure
    USART3->CR1 = 0;
    // 1 stop bit
    USART3->CR2 = 0;
    // no flow control
    USART3->CR3 = 0;
    // no prescaler
    USART3->PRESC = 0;
    // sets baud rate
    USART3->BRR = UART_KER_CLK / baud;

    // enables USART with configuration
    USART3->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    // wait until the transmitter and receiver report they are enabled
    while ((USART3->ISR & (USART_ISR_TEACK | USART_ISR_REACK))
           != (USART_ISR_TEACK | USART_ISR_REACK)) { };

    // enable interrupt on USART3 rx
    NVIC_EnableIRQ(USART3_IRQn);
}

// transmits a single char over UART, waits until the transmit register is free
void uart_putc(char c)
{
    while (!(USART3->ISR & USART_ISR_TXE_TXFNF)) { };
    USART3->TDR = c;
}

// transmits a string over UART char by char
void uart_puts(const char *s)
{
    for (uint32_t i = 0; s[i] != '\0'; i++)
    {
        uart_putc(s[i]);
    }
}

// stores incoming data in ring buffer
void USART3_IRQHandler(void)
{
    // clear overrun, will deadlock if we dont
    if (USART3->ISR & USART_ISR_ORE)
    {
        USART3->ICR = USART_ICR_ORECF;
    }

    // read rx data and store in ring buffer if not full.
    if (USART3->ISR & USART_ISR_RXNE_RXFNE)
    {
        char c = (char)USART3->RDR;
        uint32_t next = (rx_head + 1) & (RX_BUF_SIZE - 1);

        if (next != rx_tail)
        {
            rx_buf[rx_head] = c;
            rx_head = next;
        }
    }
}

// waits until a character is in the rx ring buffer and returns it
char uart_getc(void)
{
    while (!uart_rx_ready()) { };
    char c = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) & (RX_BUF_SIZE - 1);
    return c;
}

// returns a 1 if the buffer is not empty.
int uart_rx_ready(void)
{
    return rx_head != rx_tail;
}
