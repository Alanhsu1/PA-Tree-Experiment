#ifndef MY_TIMER_H
#define MY_TIMER_H

#include "FreeRTOS.h"

typedef enum TIMER_TYPE {
    HIGH_RES_CLK,
    LOW_RES_CLK
} TIMER_TYPE;

void low_res_timer_init();
void low_res_timer_start();
void high_res_timer_init();
void high_res_timer_start();
void low_res_timer_pause();
void low_res_timer_resume();
uint64_t get_current_tick(TIMER_TYPE type);
uint64_t get_elapsed_ticks(uint64_t start, uint64_t end);
double get_elapsed_time(uint64_t start, uint64_t end, TIMER_TYPE type);

#endif
