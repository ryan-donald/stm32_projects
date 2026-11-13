#include "stm32h7xx.h"
#include "uart.h"

#define UART_KER_CLK 64000000UL         // USART3 kernel clock (hsi_ker)
#define RX_BUF_SIZE 64                  // UART receive buffer, must be a power of 2
#define TX_BUF_SIZE 256                 // UART transmit buffer, must be a power of 2

static volatile uint8_t rx_buf[RX_BUF_SIZE];   // UART rx ring buffer
static volatile uint8_t tx_buf[TX_BUF_SIZE];   // UART tx ring buffer

static const uart_config_t uart3_config = {
    .uart_registers = USART3,
    .irq = USART3_IRQn,
    .rx_buf = rx_buf,
    .tx_buf = tx_buf,
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

void uart_init(uart_t *uart, uint32_t baud)
{
    USART_TypeDef *registers = uart->config->uart_registers;

    // disable USART to configure
    registers->CR1 = 0;
    // 1 stop bit
    registers->CR2 = 0;
    // no flow control
    registers->CR3 = 0;
    // no prescaler
    registers->PRESC = 0;
    // sets baud rate
    registers->BRR = UART_KER_CLK / baud;

    // enables USART with configuration
    registers->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_UE;

    // wait until the transmitter and receiver report they are enabled
    while ((registers->ISR & (USART_ISR_TEACK | USART_ISR_REACK))
           != (USART_ISR_TEACK | USART_ISR_REACK)) { };

    // enable the UART interrupt
    NVIC_EnableIRQ(uart->config->irq);
}

// queues len bytes for transmit, waits only while the tx ring buffer is full
void uart_write(uart_t *uart, const uint8_t *data, uint32_t len)
{
    uint32_t i = 0;

    while (i < len)
    {
        uint32_t head = uart->tx_head;

        // copy until the data ends or the buffer is full
        while (i < len)
        {
            uint32_t next = (head + 1) & (TX_BUF_SIZE - 1);
            if (next == uart->tx_tail)
            {
                break;
            }
            uart->config->tx_buf[head] = data[i++];
            head = next;
        }

        // publish the copied bytes and set interrupt enable bit to start sending
        uart->tx_head = head;
        uart->config->uart_registers->CR1 |= USART_CR1_TXEIE_TXFNFIE;

        // wait until there is space in the buffer if full
        while (i < len && ((uart->tx_head + 1) & (TX_BUF_SIZE - 1)) == uart->tx_tail) { };
    }
}

// queues a single char for transmit
void uart_putc(uart_t *uart, char c)
{
    uart_write(uart, (const uint8_t *)&c, 1);
}

// queues a string for transmit
void uart_puts(uart_t *uart, const char *s)
{
    uint32_t len = 0;

    // count characters up to the null terminator
    while (s[len] != '\0')
    {
        len++;
    }
    uart_write(uart, (const uint8_t *)s, len);
}

// queues a signed integer as decimal text
void uart_put_int(uart_t *uart, int32_t data)
{
    char text[11];          // sign plus at most 10 digits for a 32-bit value
    uint32_t pos = sizeof(text);
    uint32_t magnitude;

    // extract magnitude
    if (data < 0)
    {
        magnitude = -(uint32_t)data;
    }
    else
    {
        magnitude = data;
    }

    // fill digits from the end, lowest first, do-while so 0 still prints one digit
    do
    {
        text[--pos] = '0' + (magnitude % 10);
        magnitude /= 10;
    } while (magnitude != 0);

    // add sign in front of the digits
    if (data < 0)
    {
        text[--pos] = '-';
    }

    uart_write(uart, (const uint8_t *)&text[pos], sizeof(text) - pos);
}

// stores incoming data in the rx ring buffer and sends from the tx ring buffer
void uart_irq(uart_t *uart)
{
    USART_TypeDef *registers = uart->config->uart_registers;

    // clear overrun, will deadlock if we dont
    if (registers->ISR & USART_ISR_ORE)
    {
        registers->ICR = USART_ICR_ORECF;
    }

    // read rx data and store in ring buffer if not full.
    if (registers->ISR & USART_ISR_RXNE_RXFNE)
    {
        char c = (char)registers->RDR;
        uint32_t next = (uart->rx_head + 1) & (RX_BUF_SIZE - 1);

        if (next != uart->rx_tail)
        {
            uart->config->rx_buf[uart->rx_head] = c;
            uart->rx_head = next;
        }
    }

    // send next char and disable interrupt when empty
    if ((registers->CR1 & USART_CR1_TXEIE_TXFNFIE) && (registers->ISR & USART_ISR_TXE_TXFNF))
    {
        if (uart->tx_tail != uart->tx_head)
        {
            registers->TDR = uart->config->tx_buf[uart->tx_tail];
            uart->tx_tail = (uart->tx_tail + 1) & (TX_BUF_SIZE - 1);
        }

        if (uart->tx_tail == uart->tx_head)
        {
            registers->CR1 &= ~USART_CR1_TXEIE_TXFNFIE;
        }
    }
}

void USART3_IRQHandler(void)
{
    uart_irq(&uart3);
}

// waits until a character is in the rx ring buffer and returns it
char uart_getc(uart_t *uart)
{
    while (!uart_rx_ready(uart)) { };
    char c = uart->config->rx_buf[uart->rx_tail];
    uart->rx_tail = (uart->rx_tail + 1) & (RX_BUF_SIZE - 1);
    return c;
}

// returns a 1 if the buffer is not empty.
int uart_rx_ready(uart_t *uart)
{
    return uart->rx_head != uart->rx_tail;
}
