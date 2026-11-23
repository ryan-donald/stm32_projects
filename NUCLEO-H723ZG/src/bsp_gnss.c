#include "bsp_uart.h"
#include "bsp_gnss.h"

#define GNSS_BAUD 38400                // NEO-F10N default, 8N1

// NEO-F10N TX -> D0 (PB7), RX <- D1 (PB6)
static const nmea_config_t gnss_config = {
    .uart = &uart1,
};

nmea_t gnss = {
    .config = &gnss_config,
};

void gnss_init(void)
{
    uart1_init(GNSS_BAUD);
}
