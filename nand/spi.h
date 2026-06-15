#ifndef SPI_H_
#define SPI_H_

#include <stddef.h>

int spi_send(const uint8_t *send_buffer, size_t buffer_len);
int spi_recv(uint8_t *recv_buffer, size_t buffer_len);
int spi_send_recv(const uint8_t *send_buffer, uint8_t *recv_buffer, size_t buffer_len);

#endif
