#ifndef DEBUG_H
#define DEBUG_H

#include "FreeRTOS.h"
#include "driverlib.h"
#include "checkpoint.h"

#if (EXPERIMENT == EXP_ADAPTIVE) || (EXPERIMENT == EXP_LOGGING)
#define SET_BREAKPOINT(bp) { \
    breakpoint = bp; \
    P1OUT |= BIT0; \
    while (1) shutdown();       \
}
#else
#define SET_BREAKPOINT(bp) { \
    breakpoint = bp; \
    __no_operation(); \
    __no_operation(); \
}
#endif

#define SPI_ENERGY 0.081 //uJ
#define FLASH_PAGE_READ_ENERGY 2.1 // uJ
#define FLASH_PAGE_WRITE_ENERGY 2.37 //uJ

typedef enum PHASE{
    PHASE_NONE,
    PHASE_CLEAR_BLOCKS,
    PHASE_READ_DATA,
    PHASE_INSERT,
    PHASE_FLUSH,
    PHASE_SPLIT,
    PHASE_COMPACTION,
    PHASE_CHECKPOINT,
    PHASE_RESTORE,
    PHASE_GET,
    PHASE_SCAN,
    PHASE_ALL_INSERT,
    PHASE_FINISHED,
    PHASE_NUMBER
} PHASE;

typedef struct Statistics {
    uint32_t count[PHASE_NUMBER];

    uint16_t split_type_cnt[3];
    uint16_t compaction_type_cnt[3];

    uint16_t power_event_phase[PHASE_NUMBER];

    uint32_t flash_page_write_cnt[PHASE_NUMBER];
    uint32_t flash_spi_transmit_cnt[PHASE_NUMBER];
    uint32_t flash_page_read_cnt[PHASE_NUMBER];
    uint32_t flash_spi_receive_cnt[PHASE_NUMBER];
    uint16_t flash_erase_cnt;
    uint32_t flash_buffer_hit;

    uint8_t max_depth;
    uint32_t total_valid_cell_count;
    uint32_t total_cell_length;

    uint16_t get_not_found;
    uint16_t scan_found;

    double elapsed_time[PHASE_NUMBER];

    double min_time[PHASE_NUMBER];
    double max_time[PHASE_NUMBER];

    double min_energy[PHASE_NUMBER];
    double max_energy[PHASE_NUMBER];
} Statistics;

typedef enum BREAKPOINT_TYPE {
    BP_NONE,
    BP_ERROR,
    BP_TEST,
    BP_FINISHED
} BREAKPOINT_TYPE;

void init_stats();

#endif

