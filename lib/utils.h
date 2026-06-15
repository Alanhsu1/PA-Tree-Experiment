#ifndef UTILS_H
#define UTILS_H

#include "FreeRTOS.h"
#include "driverlib.h"

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))
void DMA_transfer(uint8_t *src, uint8_t *dst, size_t size);
inline int32_t rand_32bits();


#endif
