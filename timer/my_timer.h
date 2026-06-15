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
uint32_t get_current_tick(TIMER_TYPE type);
double get_elapsed_time(uint32_t start, uint32_t end, TIMER_TYPE type);

#endif
