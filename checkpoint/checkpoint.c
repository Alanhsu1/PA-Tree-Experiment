#include "driverlib.h"
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "checkpoint.h"
#include "my_timer.h"
#include "debug.h"
#include "config.h"

#define HASH(addr) ((addr) & (UNDO_LOG_HASH_SIZE - 1))

extern BREAKPOINT_TYPE breakpoint;
extern Statistics stats;
extern PHASE phase;

extern uint32_t stat_timer_1_start;
extern uint32_t stat_timer_1_end;
extern uint32_t stat_timer_2_start;
extern uint32_t stat_timer_2_end;

extern uint16_t checkpoint_countdown;

#pragma PERSISTENT(last_phase)
PHASE last_phase = PHASE_NONE;

#pragma PERSISTENT(snapshot)
Snapshot snapshot[SNAPSHOT_SLOT_COUNT] = {0};

#pragma PERSISTENT(snapshot_head)
uint8_t snapshot_head = 0;

void *snapshot_reg;

#pragma PERSISTENT(undo_log)
uint8_t undo_log[UNDO_LOG_SIZE] = {0};

#pragma PERSISTENT(undo_log_hash)
UndoLogEntryDescriptor *undo_log_hash[UNDO_LOG_HASH_SIZE] = {NULL};

#pragma PERSISTENT(undo_log_head)
uint8_t *undo_log_head = undo_log;

#pragma PERSISTENT(rtc_calendar_param)
// Calendar rtc_calendar_param = {.Seconds = 0x59,
//                                .Minutes = 0x0,
//                                .Hours = 0x0,
//                                .DayOfWeek = 0x0,
//                                .DayOfMonth = 0x0,
//                                .Month = 0x0,
//                                .Year = 0x0
//                                };
Calendar rtc_calendar_param = {.Seconds = 0x52,
                               .Minutes = 0x0,
                               .Hours = 0x0,
                               .DayOfWeek = 0x0,
                               .DayOfMonth = 0x0,
                               .Month = 0x0,
                               .Year = 0x0
                               };

extern uint8_t ucHeap[configTOTAL_HEAP_SIZE];
extern BufferEntry *rec_head;
extern BucketMetadata *bm_head;
extern Cell *cell_head;
extern AddrLevelPair *cell_level_map_head;
extern Cell cells[GRIDTREE_MAX_COUNT];
extern AddrLevelPair cell_level_map[CELL_LEVEL_MAP_MAX_SIZE];

extern void snapshotReg();
extern void restoreReg();

void checkpoint(CHECKPOINT_TYPE type)
{
    ++stats.count[PHASE_CHECKPOINT];

    Snapshot *open_snapshot = &snapshot[snapshot_head];
    open_snapshot->status = COMMIT_INCOMPLETE;
    open_snapshot->type = type;
    
    /*******************************************/
    /*              Storage space              */
    /*******************************************/
    
    portENTER_CRITICAL();
    open_snapshot->rec_head = rec_head;
    open_snapshot->bm_head = bm_head;
    open_snapshot->cell_head = cell_head;
    open_snapshot->cell_level_map_head = cell_level_map_head;
    portEXIT_CRITICAL();

    /*******************************************/
    /*              Program space              */
    /*******************************************/

    snapshot_reg = &open_snapshot->registers;

    /* Backup SRAM and registers */
    portENTER_CRITICAL();
    DMA_transfer((uint8_t *)__bss__, (uint8_t *)open_snapshot->bss, BSS_SIZE);
    DMA_transfer((uint8_t *)__data__, (uint8_t *)open_snapshot->data, DATA_SIZE);
    DMA_transfer((uint8_t *)ucHeap, (uint8_t *)open_snapshot->heap, UCHEAP_SIZE);
    snapshotReg();
    portEXIT_CRITICAL();

    /*******************************************/
    /*             Clear Undo Log              */
    /*******************************************/

    undo_log_head = undo_log;
    for (int i = 0; i < UNDO_LOG_HASH_SIZE; ++i)
        undo_log_hash[i] = NULL;

    if (phase == PHASE_RESTORE) // from restore
        return;

    /* Commit completion flag & update snapshot head */

    portENTER_CRITICAL();
    snapshot_head ^= 1;
    open_snapshot->status = COMMIT_COMPLETE;
    portEXIT_CRITICAL();
}

void restore()
{
    ++stats.count[PHASE_RESTORE];

    Snapshot *complete_snapshot = &snapshot[snapshot_head^1];

    if (complete_snapshot->status == COMMIT_NULL)
    {
        stat_timer_1_end = get_current_tick(HIGH_RES_CLK);
        stats.elapsed_time[PHASE_RESTORE] += get_elapsed_time(stat_timer_1_start, stat_timer_1_end, HIGH_RES_CLK);
        return;
    }

    if (complete_snapshot->status == COMMIT_INCOMPLETE)
        SET_BREAKPOINT(BP_ERROR);

    snapshot_reg = &complete_snapshot->registers;

    /*******************************************/
    /*              Storage space              */
    /*******************************************/
    portENTER_CRITICAL();

    undo_post_ckpt_data();
    undo_pre_ckpt_data();
    rec_head = complete_snapshot->rec_head;
    bm_head = complete_snapshot->bm_head;
    cell_head = complete_snapshot->cell_head;
    cell_level_map_head = complete_snapshot->cell_level_map_head;

    portEXIT_CRITICAL();

    /*******************************************/
    /*              Program space              */
    /*******************************************/
    portENTER_CRITICAL();

    /* Restore SRAM and registers */
    DMA_transfer((uint8_t *)complete_snapshot->bss, (uint8_t *)__bss__, BSS_SIZE);
    DMA_transfer((uint8_t *)complete_snapshot->data, (uint8_t *)__data__, DATA_SIZE);
    DMA_transfer((uint8_t *)complete_snapshot->heap, (uint8_t *)ucHeap, UCHEAP_SIZE);

    stat_timer_1_end = get_current_tick(HIGH_RES_CLK);
    double elapsed_time = get_elapsed_time(stat_timer_1_start, stat_timer_1_end, HIGH_RES_CLK);
    stats.elapsed_time[PHASE_RESTORE] += elapsed_time;

#ifdef STAT_TIME_MIN_MAX
    stats.min_time[PHASE_RESTORE] = min(stats.min_time[PHASE_RESTORE], elapsed_time);
    stats.max_time[PHASE_RESTORE] = max(stats.max_time[PHASE_RESTORE], elapsed_time);
#endif

    restoreReg();

    /* Never reach here */
    portEXIT_CRITICAL();
}

void add_undo_log_entry(UNDOLOG_ENTRY_TYPE type, uintptr_t addr)
{
    UndoLogEntryDescriptor **hash_ptr = &undo_log_hash[HASH(addr)];
    
    for (UndoLogEntryDescriptor *it = *hash_ptr; it != NULL; it = it->prev)
        if (it->addr == addr)
            return;

    UndoLogEntryDescriptor *uled_ptr = (UndoLogEntryDescriptor *)undo_log_head;

    uled_ptr->token = 0;
    uled_ptr->type = type; uled_ptr->addr = addr; uled_ptr->prev = *hash_ptr;
    *hash_ptr = uled_ptr;

    void *value_ptr = (void *)(uled_ptr + 1);

    switch (type)
    {
    case UNDOLOG_ENTRY_CELL:
        *(Cell *)value_ptr = *(Cell *)addr;
        undo_log_head += sizeof(Cell) + sizeof(UndoLogEntryDescriptor);
        break;
    case UNDOLOG_ENTRY_BM:
        *(BucketMetadata *)value_ptr = *(BucketMetadata *)addr;
        undo_log_head += sizeof(BucketMetadata) + sizeof(UndoLogEntryDescriptor);
        break;
    default:
        SET_BREAKPOINT(BP_ERROR);
        return;
    }

    uled_ptr->token = 0x87;

    if ((uintptr_t)undo_log_head > (uintptr_t)&undo_log[UNDO_LOG_SIZE])
        SET_BREAKPOINT(BP_TEST);
}

void undo_post_ckpt_data()
{
    Snapshot *complete_snapshot = &snapshot[snapshot_head ^ 1];

    if ((uintptr_t)cell_head < (uintptr_t)complete_snapshot->cell_head || (uintptr_t)cell_level_map_head < (uintptr_t)complete_snapshot->cell_level_map_head)
        SET_BREAKPOINT(BP_ERROR);

    memset(complete_snapshot->rec_head, 0, (uintptr_t)complete_snapshot->bm_head - (uintptr_t)complete_snapshot->rec_head);
    memset(complete_snapshot->cell_head, 0, (uintptr_t)cell_head - (uintptr_t)complete_snapshot->cell_head);
    memset(complete_snapshot->cell_level_map_head, 0, (uintptr_t)cell_level_map_head - (uintptr_t)complete_snapshot->cell_level_map_head);
}

void undo_pre_ckpt_data()
{
    UndoLogEntryDescriptor *uled_ptr = (UndoLogEntryDescriptor *)undo_log;

    while ((uintptr_t)uled_ptr < (uintptr_t)undo_log_head)
    {
        if (uled_ptr->token != 0x87)
            return;

        void *value_ptr = (void *)(uled_ptr + 1);

        switch (uled_ptr->type)
        {
        case UNDOLOG_ENTRY_CELL:
            *(Cell *)uled_ptr->addr = *(Cell *)value_ptr;
            uled_ptr = (UndoLogEntryDescriptor *)((uintptr_t)uled_ptr + sizeof(UndoLogEntryDescriptor) + sizeof(Cell));
            break;
        case UNDOLOG_ENTRY_BM:
            *(BucketMetadata *)uled_ptr->addr = *(BucketMetadata *)value_ptr;
            uled_ptr = (UndoLogEntryDescriptor *)((uintptr_t)uled_ptr + sizeof(UndoLogEntryDescriptor) + sizeof(BucketMetadata));
            break;
        default:
            SET_BREAKPOINT(BP_ERROR);
            return;
        }
    }
}

void shutdown()
{
    portENTER_CRITICAL();

#if EXPERIMENT == EXP_POWER_EVENT
    RTC_C_disableInterrupt(RTC_C_BASE, RTC_C_TIME_EVENT_INTERRUPT + RTC_C_CLOCK_ALARM_INTERRUPT + RTC_C_CLOCK_READ_READY_INTERRUPT + RTC_C_OSCILLATOR_FAULT_INTERRUPT);
    RTC_C_clearInterrupt(RTC_C_BASE, RTC_C_TIME_EVENT_INTERRUPT + RTC_C_CLOCK_ALARM_INTERRUPT + RTC_C_CLOCK_READ_READY_INTERRUPT + RTC_C_OSCILLATOR_FAULT_INTERRUPT);
    RTC_C_initCalendar(RTC_C_BASE, &rtc_calendar_param, RTC_C_FORMAT_BCD);
    RTC_C_setCalendarEvent(RTC_C_BASE, RTC_C_CALENDAREVENT_MINUTECHANGE);
    RTC_C_enableInterrupt(RTC_C_BASE, RTC_C_TIME_EVENT_INTERRUPT);
#endif

    PMM_turnOffRegulator();
    PMM_disableSVSH();

#if EXPERIMENT == EXP_POWER_EVENT
    RTC_C_startClock(RTC_C_BASE);
#endif
    LPM4;

    portEXIT_CRITICAL();
}

inline void CHECKPOINT(CHECKPOINT_TYPE type)
{
    phase = PHASE_CHECKPOINT;
    checkpoint_countdown = CHECKPOINT_INTERVAL;

    portENTER_CRITICAL();
    high_res_timer_start();
    stat_timer_2_start = get_current_tick(HIGH_RES_CLK);
    checkpoint(type);
    if (phase == PHASE_CHECKPOINT)
    {
        stat_timer_2_end = get_current_tick(HIGH_RES_CLK);
        double elapsed_time = get_elapsed_time(stat_timer_2_start, stat_timer_2_end, HIGH_RES_CLK);
        stats.elapsed_time[PHASE_CHECKPOINT] += elapsed_time;
#ifdef STAT_TIME_MIN_MAX
        stats.min_time[PHASE_CHECKPOINT] = min(stats.min_time[PHASE_CHECKPOINT], elapsed_time);
        stats.max_time[PHASE_CHECKPOINT] = max(stats.max_time[PHASE_CHECKPOINT], elapsed_time);
#endif
    }
    portEXIT_CRITICAL();

}

inline void RESTORE()
{
    stats.power_event_phase[phase]++;
    phase = PHASE_RESTORE;

    high_res_timer_start();
    stat_timer_1_start = get_current_tick(HIGH_RES_CLK);
    restore();
}

#pragma vector=RTC_VECTOR
__interrupt void RTC_ISR( void )
{
    RTC_C_clearInterrupt(RTC_C_BASE, RTC_C_TIME_EVENT_INTERRUPT + RTC_C_CLOCK_ALARM_INTERRUPT + RTC_C_CLOCK_READ_READY_INTERRUPT + RTC_C_OSCILLATOR_FAULT_INTERRUPT);
}

// For time based power event experiment
void setup_power_event_timer()
{
    Timer_A_initUpModeParam initUpParam = {0};
    initUpParam.clockSource = TIMER_A_CLOCKSOURCE_ACLK;

    /* To configure the power event interval, compute and set these two values down below */
    initUpParam.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_64;
    initUpParam.timerPeriod = (CS_getACLK() >> 6) * POWER_EVENT_INTERVAL;

    initUpParam.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE;
    initUpParam.captureCompareInterruptEnable_CCR0_CCIE = TIMER_A_CCIE_CCR0_INTERRUPT_DISABLE;
    initUpParam.timerClear = TIMER_A_DO_CLEAR;
    initUpParam.startTimer = false;
    Timer_A_initUpMode(TIMER_A3_BASE, &initUpParam);

    Timer_A_clear(TIMER_A3_BASE);
    Timer_A_startCounter(TIMER_A3_BASE, TIMER_A_UP_MODE);
}

void disable_power_event_timer()
{
    Timer_A_stop(TIMER_A3_BASE);
}

#pragma vector=TIMER3_A1_VECTOR
__interrupt void A3_ISR( void )
{
    TA3CTL &= ~TAIFG;

    last_phase = phase;
    stats.power_event_phase[last_phase]++;
    shutdown();

    __bic_SR_register_on_exit( SCG1 + SCG0 + OSCOFF + CPUOFF );
}
