#include "debug.h"
#include <string.h>
#include <float.h>

#pragma PERSISTENT(breakpoint)
BREAKPOINT_TYPE breakpoint = BP_NONE;

#pragma PERSISTENT(stats)
Statistics stats = {0};

#pragma NOINIT(cell_length_array)
uint8_t cell_length_array[2000];

void init_stats()
{
    for (uint8_t i = 0; i < PHASE_NUMBER; ++i)
    {
        stats.min_time[i] = DBL_MAX;
        stats.min_energy[i] = DBL_MAX;
    }
}