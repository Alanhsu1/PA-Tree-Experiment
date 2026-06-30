#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdlib.h>
#include "queue.h"
#include "checkpoint.h"
#include "gridtree.h"
#include "my_timer.h"
#include "utils.h"
#include "debug.h"
#include "config.h"
#include "query.h"

#define tskTEST_PRIORITY (tskIDLE_PRIORITY + 1)

extern Record query_records[SCAN_COUNT / 2];
extern Record scan_query_records[5];
extern uint32_t query_records_head;
extern uint32_t buffered_records_cnt;
extern uint32_t tree_data_usage[INSERT_COUNT/100];

#pragma NOINIT(workload_page)
uint8_t workload_page[PAGE_SIZE];

#pragma PERSISTENT(workload_page_head)
uint16_t workload_page_head = 65534;

#pragma NOINIT(workload_int_head)
int16_t *workload_int_head;

#pragma PERSISTENT(checkpoint_countdown)
uint16_t checkpoint_countdown = CHECKPOINT_INTERVAL;

#pragma PERSISTENT(insert_head)
uint32_t insert_head = 0;

#pragma PERSISTENT(get_head)
uint32_t get_head = 0;

#pragma PERSISTENT(scan_head)
uint32_t scan_head = 0;

#pragma PERSISTENT(scan_query_elapsed_time)
double scan_query_elapsed_time[SCAN_COUNT] = {0};

#pragma NOINIT(d)
Record d;

#pragma NOINIT(stat_timer_1_start)
uint32_t stat_timer_1_start;
#pragma NOINIT(stat_timer_1_end)
uint32_t stat_timer_1_end;
#pragma NOINIT(stat_timer_2_start)
uint32_t stat_timer_2_start;
#pragma NOINIT(stat_timer_2_end)
uint32_t stat_timer_2_end;

#pragma PERSISTENT(phase)
PHASE phase = PHASE_NONE;

extern BREAKPOINT_TYPE breakpoint;
extern Statistics stats;

void gridtree_test();

void main_gridtree()
{
    xTaskCreate(gridtree_test, "test", configMINIMAL_STACK_SIZE, NULL, tskTEST_PRIORITY, NULL);

    vTaskStartScheduler();
}

void gridtree_test()
{
    uint32_t start, end;
    uint32_t query_start, query_end;

    CHECKPOINT(CHECKPOINT_INIT);

    init();

    start = get_current_tick(LOW_RES_CLK);
    while (insert_head < INSERT_COUNT)
    {
        phase = PHASE_READ_DATA;

#if SAMPLE_RATE > 0
       while (buffered_records_cnt == 0);
#endif

        // low_res_timer_pause();

#if (DISTRIBUTION == 0)
        d.x = rand_32bits() % (ACTUAL_X_MAX - ACTUAL_X_MIN) + ACTUAL_X_MIN;
        d.y = rand_32bits() % (ACTUAL_Y_MAX - ACTUAL_Y_MIN) + ACTUAL_Y_MIN;
#elif (DISTRIBUTION == 1)
       if ((uintptr_t)workload_int_head >= (uintptr_t)&workload_page[PAGE_SIZE] || insert_head == 0)
       {
           CHECKPOINT(CHECKPOINT_READ_DATA);

           read_op(workload_page_head, 0, workload_page, PAGE_SIZE);
           workload_page_head = (workload_page_head == 65515 ? 65534 : workload_page_head-1);
           workload_int_head = (int16_t *)workload_page;
       }

       d.x = (int32_t)(*workload_int_head) * 1000 + rand() % 1000;
       d.y = (int32_t)(*(workload_int_head+1)) * 1000 + rand() % 1000; workload_int_head += 2;
#endif

    //    low_res_timer_resume();

       d.timestamp = insert_head;

       while (INSERT(&d) != RET_SUCCESS)
       {
           FLUSH();

           SPLIT();

           COMPACTION();
       }

#if SAMPLE_RATE > 0
       --buffered_records_cnt;
#endif

       if (--checkpoint_countdown == 0)
           CHECKPOINT(CHECKPOINT_INSERT);

       ++insert_head;

#if EXPERIMENT == EXP_CELL_STATS
       if ((insert_head % (INSERT_COUNT/100)) == 0)
            tree_data_usage[insert_head / (INSERT_COUNT/100)] = tree_data_used();
#endif
    }

    end = get_current_tick(LOW_RES_CLK);
    stats.elapsed_time[PHASE_ALL_INSERT] = get_elapsed_time(start, end, LOW_RES_CLK);
    stats.elapsed_time[PHASE_INSERT] = get_elapsed_time(0, stats.elapsed_time[PHASE_INSERT], HIGH_RES_CLK);
    stats.min_time[PHASE_INSERT] = get_elapsed_time(0, stats.min_time[PHASE_INSERT], HIGH_RES_CLK);
    stats.max_time[PHASE_INSERT] = get_elapsed_time(0, stats.max_time[PHASE_INSERT], HIGH_RES_CLK);

#if EXPERIMENT != EXP_CELL_STATS

    collect_scan_query_record();

    Record r1, r2;
    start = get_current_tick(LOW_RES_CLK);

//    SET_BREAKPOINT(BP_TEST);

    CHECKPOINT(CHECKPOINT_SCAN);

    while (scan_head < SCAN_COUNT)
    {
        r1 = query_records[(scan_head / 2) % query_records_head];
        r2.x = min(r1.x + SCAN_XY_RANGE + (rand() % SCAN_XY_RANGE), X_MAX);
        r2.y = min(r1.y + SCAN_XY_RANGE + (rand() % SCAN_XY_RANGE), Y_MAX);

        if ((scan_head & 1) == 0)
            r2.timestamp = min(r1.timestamp + SCAN_TIME_RANGE + (rand() % SCAN_TIME_RANGE), INSERT_COUNT);
        else
            r2.timestamp = INSERT_COUNT;

        query_start = get_current_tick(LOW_RES_CLK);
        SCAN(&r1, &r2);
        query_end = get_current_tick(LOW_RES_CLK);

        scan_query_elapsed_time[scan_head] = get_elapsed_time(query_start, query_end, LOW_RES_CLK);
        ++scan_head;
    }

    end = get_current_tick(LOW_RES_CLK);
    stats.elapsed_time[PHASE_SCAN] = get_elapsed_time(start, end, LOW_RES_CLK);
#endif

#if EXPERIMENT == EXP_CELL_STATS
    GRIDTREE_STATS();
#endif

#if EXPERIMENT == EXP_POWER_EVENT
    disable_power_event_timer();
#endif

    phase = PHASE_FINISHED;
    CHECKPOINT(CHECKPOINT_FINISHED);

    P1OUT |= BIT0; // light up the LED

    SET_BREAKPOINT(BP_FINISHED);

    for (;;);
}
