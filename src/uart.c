#include "stm32h7xx.h"
#include "uart.h"

#define UART_KER_CLK 64000000UL         // USART3 kernel clock (hsi_ker)
#define RX_BUF_SIZE 64                  // UART receive buffer, must be a power of 2

static volatile uint8_t rx_buf[RX_BUF_SIZE];   // UART rx ring buffer

uart_t uart3 = {
    .uart_registers = USART3,
    .irq = USART3_IRQn,
    .rx_buf = rx_buf,                   // UART rx ring buffer
    .rx_head = 0,                      // next slot the interrupt writes
    .rx_tail = 0,                      // next slot uart_getc reads
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

void uart_init(uart_t *uart_config, uint32_t baud)
{
    
    // disable USART to configure
    uart_config->uart_registers->CR1 = 0;
    // 1 stop bit
    uart_config->uart_registers->CR2 = 0;
    // no flow control
    uart_config->uart_registers->CR3 = 0;
    // no prescaler
    uart_config->uart_registers->PRESC = 0;
    // sets baud rate
    uart_config->uart_registers->BRR = UART_KER_CLK / baud;

    // enables USART with configuration
    uart_config->uart_registers->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    // wait until the transmitter and receiver report they are enabled
    while ((uart_config->uart_registers->ISR & (USART_ISR_TEACK | USART_ISR_REACK))
           != (USART_ISR_TEACK | USART_ISR_REACK)) { };

    // enable interrupt on USART3 rx
    NVIC_EnableIRQ(uart_config->irq);
}

// transmits a single char over UART, waits until the transmit register is free
void uart_putc(uart_t *uart_config, char c)
{
    while (!(uart_config->uart_registers->ISR & USART_ISR_TXE_TXFNF)) { };
    uart_config->uart_registers->TDR = c;
}

// transmits a string over UART char by char
void uart_puts(uart_t *uart_config, const char *s)
{
    for (uint32_t i = 0; s[i] != '\0'; i++)
    {
        uart_putc(uart_config, s[i]);
    }
}

// transmits a signed integer as decimal text
void uart_put_int(uart_t *uart_config, int32_t data)
{
    char digits[10];        // a 32-bit value has at most 10 digits
    uint32_t magnitude;
    uint32_t len = 0;

    // for 0 just print it to uart and skip logic
    if (data == 0)
    {
        uart_putc(uart_config, '0');
        return;
    }

    // print sign and extract magnitude
    if (data < 0)
    {
        uart_putc(uart_config, '-');
        magnitude = -(uint32_t)data;
    }
    else
    {
        magnitude = data;
    }

    // extract one digit at a time, stored lowest first
    while (magnitude != 0)
    {
        digits[len++] = '0' + (magnitude % 10);
        magnitude /= 10;
    }

    // transmit digits from len down to 0
    while (len > 0)
    {
        uart_putc(uart_config, digits[--len]);
    }
}

// stores incoming data in ring buffer
void uart_irq(uart_t *uart_config)
{
    // clear overrun, will deadlock if we dont
    if (uart_config->uart_registers->ISR & USART_ISR_ORE)
    {
        uart_config->uart_registers->ICR = USART_ICR_ORECF;
    }

    // read rx data and store in ring buffer if not full.
    if (uart_config->uart_registers->ISR & USART_ISR_RXNE_RXFNE)
    {
        char c = (char)uart_config->uart_registers->RDR;
        uint32_t next = (uart_config->rx_head + 1) & (RX_BUF_SIZE - 1);

        if (next != uart_config->rx_tail)
        {
            uart_config->rx_buf[uart_config->rx_head] = c;
            uart_config->rx_head = next;
        }
    }
}

void USART3_IRQHandler(void)
{
    uart_irq(&uart3);
}

// waits until a character is in the rx ring buffer and returns it
char uart_getc(uart_t *uart_config)
{
    while (!uart_rx_ready(uart_config)) { };
    char c = uart_config->rx_buf[uart_config->rx_tail];
    uart_config->rx_tail = (uart_config->rx_tail + 1) & (RX_BUF_SIZE - 1);
    return c;
}

// returns a 1 if the buffer is not empty.
int uart_rx_ready(uart_t *uart_config)
{
    return uart_config->rx_head != uart_config->rx_tail;
}
