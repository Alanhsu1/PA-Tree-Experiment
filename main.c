/*
 * FreeRTOS V202107.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/******************************************************************************
 * This project provides two demo applications.  A simple blinky style project,
 * and a more comprehensive test and demo application.  The
 * CHECKPOINT_TEST setting (defined in this file) is used to
 * select between the two.  The simply blinky demo is implemented and described
 * in main_blinky.c.  The more comprehensive test and demo application is
 * implemented and described in main_full.c.
 *
 * This file implements the code that is not demo specific, including the
 * hardware setup and standard FreeRTOS hook functions.
 *
 * ENSURE TO READ THE DOCUMENTATION PAGE FOR THIS PORT AND DEMO APPLICATION ON
 * THE http://www.FreeRTOS.org WEB SITE FOR FULL INFORMATION ON USING THIS DEMO
 * APPLICATION, AND ITS ASSOCIATE FreeRTOS ARCHITECTURE PORT!
 *
 */

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Standard demo includes, used so the tick hook can exercise some FreeRTOS
 functionality in an interrupt. */
#include "EventGroupsDemo.h"
#include "TaskNotify.h"
//#include "ParTest.h" /* LEDs - a historic name for "Parallel Port". */

/* TI includes. */
#include "driverlib.h"
#include <stdio.h>
#include <stdlib.h>

#include "checkpoint.h"
#include "my_timer.h"
#include "spi_nand.h"
#include "gridtree.h"
#include "debug.h"
#include "config.h"
#include "adc.h"
#include "workload.h"
/*-----------------------------------------------------------*/

/*
 * Configure the hardware as necessary to run this demo.
 */
static int prvSetupHardware(void);

#if EXPERIMENT == EXP_GEN_WORKLOAD
extern void main_workload();
#else
extern void main_gridtree(void);
#endif 

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
 within this file. */
void vApplicationMallocFailedHook(void);
void vApplicationIdleHook(void);
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName);
void vApplicationTickHook(void);

/* The heap is allocated here so the "persistent" qualifier can be used.  This
 requires configAPPLICATION_ALLOCATED_HEAP to be set to 1 in FreeRTOSConfig.h.
 See http://www.freertos.org/a00111.html for more information. */
#ifdef __ICC430__
	__persistent 					/* IAR version. */
#else
#pragma PERSISTENT( ucHeap ) 	/* CCS version. */
#endif
uint8_t ucHeap[configTOTAL_HEAP_SIZE] = { 0 };

extern DMA_initParam dma_param;

extern PHASE phase;

#pragma PERSISTENT(clear_all_blocks_flag)
uint8_t clear_all_blocks_flag = 0;

#pragma PERSISTENT(halt_flag)
uint8_t halt_flag = 1;

static void halt()
{
    P1DIR |= 0x0003; // Set P1.0 and P1.1 to output direction

    PM5CTL0 &= ~LOCKLPM5; // Disable the GPIO power-on default high-impedance mode
    // to activate previously configured port settings

    P1OUT = 0x0000;
    P5DIR &= ~BIT5;
    P5REN |= BIT5;

    P1OUT |= BIT0; // light up the LED

    while (1)
    {
        if (!(P5IN & BIT5))
        {
            P1OUT &= ~BIT0;
            halt_flag = 0;
            break;
        }
    }
}

/*-----------------------------------------------------------*/

int main(void)
{
    /* See http://www.FreeRTOS.org/MSP430FR5969_Free_RTOS_Demo.html */

    /* Configure the hardware ready to run the demo. */
    if (prvSetupHardware())
    {
        printf("ERROR\n");
        while (1);
    }

    if (clear_all_blocks_flag == 0)
    {
        phase = PHASE_CLEAR_BLOCKS;
        clear_all_blocks();
        clear_all_blocks_flag = 1;
    }

#if (EXPERIMENT == EXP_ADAPTIVE) || (EXPERIMENT == EXP_LOGGING)
     if (halt_flag == 1)
         halt();
#endif

#if EXPERIMENT == EXP_GEN_WORKLOAD
    main_workload();
#else
    RESTORE(); // Must be placed after HW setup since CLK, DMA, SPI need to be init.

    main_gridtree();
#endif
    return 0;
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
     free memory available in the FreeRTOS heap.  pvPortMalloc() is called
     internally by FreeRTOS API functions that create tasks, queues, software
     timers, and semaphores.  The size of the FreeRTOS heap is set by the
     configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

    /* Force an assert. */
    configASSERT(( volatile void * ) NULL);
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
    (void) pcTaskName;
    (void) pxTask;

    /* Run time stack overflow checking is performed if
     configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     function is called if a stack overflow is detected.
     See http://www.freertos.org/Stacks-and-stack-overflow-checking.html */

    /* Force an assert. */
    configASSERT(( volatile void * ) NULL);
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
    __bis_SR_register( LPM4_bits + GIE);
    __no_operation();
}
/*-----------------------------------------------------------*/

void vApplicationTickHook(void)
{
    // #if( CHECKPOINT_TEST == 0 )
    // {
    // 	/* Call the periodic event group from ISR demo. */
    // 	vPeriodicEventGroupsProcessing();

    // 	/* Call the code that 'gives' a task notification from an ISR. */
    // 	xNotifyTaskFromISR();
    // }
    // #endif
}
/*-----------------------------------------------------------*/

/* The MSP430X port uses this callback function to configure its tick interrupt.
 This allows the application to choose the tick interrupt source.
 configTICK_VECTOR must also be set in FreeRTOSConfig.h to the correct
 interrupt vector for the chosen tick interrupt source.  This implementation of
 vApplicationSetupTimerInterrupt() generates the tick from timer A0, so in this
 case configTICK_VECTOR is set to TIMER0_A0_VECTOR. */
void vApplicationSetupTimerInterrupt(void)
{
    const unsigned short usACLK_Frequency_Hz = 32768;

    /* Ensure the timer is stopped. */
    TA0CTL = 0;

    /* Run the timer from the ACLK. */
    TA0CTL = TASSEL_1;

    /* Clear everything to start with. */
    TA0CTL |= TACLR;

    /* Set the compare match value according to the tick rate we want. */
    TA0CCR0 = usACLK_Frequency_Hz / configTICK_RATE_HZ;

    /* Enable the interrupts. */
    TA0CCTL0 = CCIE;

    /* Start up clean. */
    TA0CTL |= TACLR;

    /* Up mode. */
    TA0CTL |= MC_1;
}
/*-----------------------------------------------------------*/

static int prvSetupHardware(void)
{
    /* Stop Watchdog timer. */
    WDT_A_hold( __MSP430_BASEADDRESS_WDT_A__);

    /* Disable RTC */
    RTC_C_holdClock(RTC_C_BASE);

    /* Set PJ.4 and PJ.5 for LFXT. */
    GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_PJ,
                                               GPIO_PIN4 + GPIO_PIN5,
                                               GPIO_PRIMARY_MODULE_FUNCTION);

    /* Set DCO frequency to 8 MHz. */
    CS_setDCOFreq( CS_DCORSEL_0, CS_DCOFSEL_6);

    /* Set external clock frequency to 32.768 KHz. */
    CS_setExternalClockSource(32768, 0);

    /* Set ACLK = LFXT. */
    CS_initClockSignal( CS_ACLK, CS_LFXTCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Set SMCLK = DCO with frequency divider of 1. */
    CS_initClockSignal( CS_SMCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Set MCLK = DCO with frequency divider of 1. */
    CS_initClockSignal( CS_MCLK, CS_DCOCLK_SELECT, CS_CLOCK_DIVIDER_1);

    /* Start XT1 with no time out. */
    CS_turnOnLFXT( CS_LFXT_DRIVE_0);

    /* Disable the GPIO power-on default high-impedance mode. */
    PMM_unlockLPM5();

    /* Setup DMA */
    dma_param.channelSelect = DMA_CHANNEL_0;
    dma_param.transferModeSelect = DMA_TRANSFER_BLOCK;
    dma_param.transferUnitSelect = DMA_SIZE_SRCWORD_DSTWORD;
    DMA_init(&dma_param);

    /* Setup Timer */
    low_res_timer_init();
    low_res_timer_start();
    high_res_timer_init();

    /* Setup FreeRTOS tick timer */
    vApplicationSetupTimerInterrupt();

#ifdef POWER_EVENT_ON
    setup_power_event_timer();
#endif

#if (EXPERIMENT == EXP_ADAPTIVE) || (EXPERIMENT == EXP_LOGGING)
    init_adc();
#endif

#if SAMPLE_RATE > 0
    setup_sample_rate_timer();
#endif

    /* Setup SPI */
    GPIO_setAsPeripheralModuleFunctionInputPin(
            GPIO_PORT_P5, GPIO_PIN0 | GPIO_PIN1 | GPIO_PIN2,
            GPIO_PRIMARY_MODULE_FUNCTION);
    GPIO_setAsOutputPin(GPIO_PORT_P4, GPIO_PIN1);
    GPIO_setOutputHighOnPin(GPIO_PORT_P4, GPIO_PIN1);
    EUSCI_B_SPI_initMasterParam spi_init = {
            .selectClockSource = EUSCI_B_SPI_CLOCKSOURCE_SMCLK,
            .clockSourceFrequency = 8000000, .desiredSpiClock = 8000000,
            .msbFirst = EUSCI_B_SPI_MSB_FIRST, .clockPhase =
            EUSCI_B_SPI_PHASE_DATA_CAPTURED_ONFIRST_CHANGED_ON_NEXT,
            .clockPolarity = EUSCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW,
            .spiMode = EUSCI_B_SPI_3PIN };
    EUSCI_B_SPI_initMaster(EUSCI_B1_BASE, &spi_init);
    EUSCI_B_SPI_enable(EUSCI_B1_BASE);

    /* Button P5.6 */
    GPIO_selectInterruptEdge(GPIO_PORT_P5, GPIO_PIN6, GPIO_HIGH_TO_LOW_TRANSITION);
    GPIO_clearInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_enableInterrupt(GPIO_PORT_P5, GPIO_PIN6);
    GPIO_setAsInputPinWithPullUpResistor(GPIO_PORT_P5, GPIO_PIN6);

    /* LED 1.0 */
    GPIO_setAsOutputPin(GPIO_PORT_P1, GPIO_PIN0);
    GPIO_setOutputLowOnPin(GPIO_PORT_P1, GPIO_PIN0);

    return spi_nand_init();
}
/*-----------------------------------------------------------*/

int _system_pre_init(void)
{
    /* Stop Watchdog timer. */
    WDT_A_hold( __MSP430_BASEADDRESS_WDT_A__);

    /* Return 1 for segments to be initialised. */
    return 1;
}

