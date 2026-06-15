#include "my_timer.h"
#include "FreeRTOS.h"
#include "driverlib.h"

#pragma PERSISTENT(elapsed_tick)
uint32_t elapsed_tick = 0;

// #define HIGH_RES_CLK TIMER_A_CLOCKSOURCE_SMCLK
// #define LOW_RES_CLK TIMER_A_CLOCKSOURCE_ACLK
// #define TIMER_CLK_SOURCE LOW_RES_CLK

void low_res_timer_init()
{
    Timer_A_initContinuousModeParam initContParam = {0};
    initContParam.clockSource = TIMER_A_CLOCKSOURCE_ACLK;
    initContParam.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_1;
    initContParam.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_ENABLE;
    initContParam.timerClear = TIMER_A_DO_CLEAR;
    initContParam.startTimer = false;
    Timer_A_initContinuousMode(TIMER_A2_BASE, &initContParam);

    Timer_A_enableInterrupt(TIMER_A2_BASE);
}

void high_res_timer_init()
{
    Timer_A_initContinuousModeParam initContParam = {0};
    initContParam.clockSource = TIMER_A_CLOCKSOURCE_SMCLK;
    initContParam.clockSourceDivider = TIMER_A_CLOCKSOURCE_DIVIDER_2;
    initContParam.timerInterruptEnable_TAIE = TIMER_A_TAIE_INTERRUPT_DISABLE;
    initContParam.timerClear = TIMER_A_DO_CLEAR;
    initContParam.startTimer = false;
    Timer_A_initContinuousMode(TIMER_A1_BASE, &initContParam);

    Timer_A_disableInterrupt(TIMER_A1_BASE);
}

double get_elapsed_time(uint32_t start, uint32_t end, TIMER_TYPE type)
{
    if (end < start)
        return 0;
    
    return (double)(end - start) / (type == HIGH_RES_CLK ? CS_getSMCLK() >> 1 : CS_getACLK());
}

void low_res_timer_start()
{
    Timer_A_clear(TIMER_A2_BASE);
    Timer_A_startCounter(TIMER_A2_BASE, TIMER_A_CONTINUOUS_MODE);
}

void high_res_timer_start()
{
    Timer_A_clear(TIMER_A1_BASE);
    Timer_A_startCounter(TIMER_A1_BASE, TIMER_A_CONTINUOUS_MODE);
}

uint32_t get_current_tick(TIMER_TYPE type)
{
    return (type == HIGH_RES_CLK ? (uint32_t)Timer_A_getCounterValue(TIMER_A1_BASE) : (uint32_t)Timer_A_getCounterValue(TIMER_A2_BASE) + elapsed_tick);
}

void low_res_timer_pause()
{
    Timer_A_stop(TIMER_A2_BASE);
}

void low_res_timer_resume()
{
    Timer_A_startCounter(TIMER_A2_BASE, TIMER_A_CONTINUOUS_MODE);
}

#pragma vector=TIMER2_A1_VECTOR
__interrupt void A2_ISR( void )
{
    if (TA2IV == 0xE)
        elapsed_tick += 0x10000;

    TA2CTL &= ~TAIFG;
	
	__bic_SR_register_on_exit( SCG1 + SCG0 + OSCOFF + CPUOFF );
}
