#include "FreeRTOS.h"
#include "driverlib.h"
#include "spi.h"

int spi_send(const uint8_t* send_buffer, size_t buffer_len)
{
    int i;
    volatile uint8_t dummy;
    for (i = 0; i < buffer_len; ++i)
    {
        while (!(UCB1IFG & UCTXIFG)); 
        UCB1TXBUF = send_buffer[i]; 
        while (!(UCB1IFG & UCRXIFG));
        dummy = UCB1RXBUF;
    }

    return 0;
}

int spi_recv(uint8_t *recv_buffer, size_t buffer_len)
{
    int i;
    for (i = 0; i < buffer_len; ++i)
    {
        while (!(UCB1IFG & UCTXIFG));
        UCB1TXBUF = 0; 
        while (!(UCB1IFG & UCRXIFG)); 
        recv_buffer[i] = UCB1RXBUF; 
    }

    return 0;
}

int spi_send_recv(const uint8_t* send_buffer, uint8_t *recv_buffer, size_t buffer_len)
{
    int i;
    for (i = 0; i < buffer_len; ++i)
    {
        while (!(UCB1IFG & UCTXIFG));
        UCB1TXBUF = send_buffer[i]; 
        while (!(UCB1IFG & UCRXIFG));
        recv_buffer[i] = UCB1RXBUF; 
    }

    return 0;
}
