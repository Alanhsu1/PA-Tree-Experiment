#ifndef CHECKPOINT_H
#define CHECKPOINT_H

#include "FreeRTOS.h"
#include "gridtree.h"
#include "config.h"

#define SNAPSHOT_OPTIMIZATION

typedef enum CHECKPOINT_TYPE{
    CHECKPOINT_INIT = 1,
    CHECKPOINT_READ_DATA,
    CHECKPOINT_INSERT,
    CHECKPOINT_FLUSH,
    CHECKPOINT_SPLIT,
    CHECKPOINT_COMPACTION,
    CHECKPOINT_GET,
    CHECKPOINT_SCAN,
    CHECKPOINT_FINISHED
} CHECKPOINT_TYPE;

enum COMMIT_STATUS {
    COMMIT_NULL       = 0x00,
    COMMIT_INCOMPLETE = 0x87, 
    COMMIT_COMPLETE   = 0xAA
};

typedef enum UNDOLOG_ENTRY_TYPE {
    UNDOLOG_ENTRY_CELL = 0x00,
    UNDOLOG_ENTRY_BM,
} UNDOLOG_ENTRY_TYPE;

extern char __bss__[], __bssEnd__[], __data__[], __dataEnd__[];


#define BSS_SIZE (__bssEnd__ - __bss__)
#define DATA_SIZE (__dataEnd__ - __data__)
// #define STACK_SIZE                (__stackEnd__ - __stack__)
#ifdef SNAPSHOT_OPTIMIZATION
#define UCHEAP_SIZE 1000
#else
#define UCHEAP_SIZE (configTOTAL_HEAP_SIZE)
#endif

#define SNAPSHOT_SLOT_COUNT     2
#define SNAPSHOT_BSS_SIZE       1024
#define SNAPSHOT_DATA_SIZE      1024
// #define SNAPSHOT_STACK_SIZE     1024
#define UNDO_LOG_SIZE           20000
#define UNDO_LOG_HASH_SIZE      512 // Must be power of 2

typedef struct Snapshot {
    /* program context */
    uint32_t registers[16];
    uint8_t bss[SNAPSHOT_BSS_SIZE];
    uint8_t data[SNAPSHOT_DATA_SIZE];
    // uint8_t stack[STACK_SIZE];
    uint8_t heap[UCHEAP_SIZE];

    /* Tree context */
    BufferEntry *rec_head;
    BucketMetadata *bm_head;
    Cell *cell_head;
    AddrLevelPair *cell_level_map_head;

    /* informations */
    CHECKPOINT_TYPE type;
    uint8_t status;
} Snapshot;

typedef struct UndoLogEntryDescriptor {
    struct UndoLogEntryDescriptor *prev;
    UNDOLOG_ENTRY_TYPE type;
    uintptr_t addr;
    uint8_t token;
} UndoLogEntryDescriptor;

/* Checkpoint */
void checkpoint(CHECKPOINT_TYPE type);
void restore();
void add_undo_log_entry(UNDOLOG_ENTRY_TYPE type, uintptr_t addr);
void undo_post_ckpt_data();
void undo_pre_ckpt_data();

/* Power event simulation */
void shutdown();
void setup_power_event_timer();
void disable_power_event_timer();

/* Wrappers */
inline void CHECKPOINT(CHECKPOINT_TYPE type);
inline void RESTORE();

#endif
