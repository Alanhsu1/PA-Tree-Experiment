#include "FreeRTOS.h"
#include "gridtree.h"
#include "driverlib.h"
#include "utils.h"

#pragma PERSISTENT(dma_param)
DMA_initParam dma_param = {0};

void DMA_transfer(uint8_t *src, uint8_t *dst, size_t size)
{
    DMA_setTransferSize(dma_param.channelSelect, (size + 1) / 2); // Byte -> Word
    DMA_setSrcAddress(dma_param.channelSelect, (uint32_t)src, DMA_DIRECTION_INCREMENT);
    DMA_setDstAddress(dma_param.channelSelect, (uint32_t)dst, DMA_DIRECTION_INCREMENT);
    DMA_enableTransfers(dma_param.channelSelect);

    DMA_startTransfer(dma_param.channelSelect);
}

inline int32_t rand_32bits()
{
    return (int32_t)((uint32_t)rand() | ((uint32_t)rand() << 16) & ~(1L << 31));
}