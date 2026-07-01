#ifndef CONFIG_H
#define CONFIG_H

#define EXP_GEN_WORKLOAD 0
#define EXP_DIFF_EVAL_SPLIT 1
#define EXP_DIFF_EVAL_COMPACTION 2
#define EXP_POWER_EVENT 3
/*--------TESTBED--------*/
#define EXP_LOGGING 4
#define EXP_ADAPTIVE 5
/*--------TESTBED--------*/
#define EXP_TEST 6
#define EXP_CELL_STATS 7

#define EXPERIMENT EXP_ADAPTIVE

/* Input */
#define DISTRIBUTION        1  // 0: uniform, 1: normal
#define INSERT_COUNT        300000L
#define GET_COUNT           500L
#define SCAN_COUNT          40
#define SCAN_BUFFER_SIZE    500
#define SESSION_QUERY_SEPARATION_SECONDS 4.0
#define AVG_INSERT_TIME_SECONDS 0.00025164
#define SAMPLE_RATE         800L // 0 to turnoff
#define STAT_TIME_MIN_MAX   
#define STAT_ENERGY_MIN_MAX 

#if DISTRIBUTION == 0
#define SCAN_XY_RANGE       300000
#else
#define SCAN_XY_RANGE       10000
#endif
#define SCAN_TIME_RANGE     10000

#if DISTRIBUTION == 0
#define AMPLE_ENERGY_VOLTAGE_THRESHOLD 3.35
#define VOLTAGE_DROP_THRESHOLD 0.00
#else
#define AMPLE_ENERGY_VOLTAGE_THRESHOLD 3.45
#define VOLTAGE_DROP_THRESHOLD 0.000
#endif

/* Gridtree */
#if (EXPERIMENT == EXP_DIFF_EVAL_SPLIT) || (EXPERIMENT == EXP_DIFF_EVAL_COMPACTION)
#define MAX_FLASH_BLOCK 256
#else
#define MAX_FLASH_BLOCK 64
#endif

#define CELL_CAPACITY 500

/* Checkpoint */
#define CHECKPOINT_INTERVAL 500L
#if EXPERIMENT == EXP_POWER_EVENT
#define POWER_EVENT_ON
#endif
#define POWER_EVENT_INTERVAL 80

#endif
