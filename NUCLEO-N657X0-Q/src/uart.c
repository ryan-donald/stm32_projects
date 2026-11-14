#include "stm32n6xx.h"
#include "uart.h"

#define UART_KER_CLK 64000000UL         // USART1 kernel clock (hsi_div_ck)
#define RX_BUF_SIZE 64                  // UART receive buffer, must be a power of 2
#define TX_BUF_SIZE 256                 // UART transmit buffer, must be a power of 2

static volatile uint8_t rx_buf[RX_BUF_SIZE];   // UART rx ring buffer
static volatile uint8_t tx_buf[TX_BUF_SIZE];   // UART tx ring buffer

static const uart_config_t uart1_config = {
    .uart_registers = USART1,
    .irq = USART1_IRQn,
    .rx_buf = rx_buf,
    .tx_buf = tx_buf,
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

void USART1_IRQHandler(void)
{
    uart_irq(&uart1);
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
