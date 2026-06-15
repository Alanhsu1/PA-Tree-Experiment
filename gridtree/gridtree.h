#ifndef GRIDTREE_H
#define GRIDTREE_H

#include "FreeRTOS.h"
#include "spi_nand.h"
#include "config.h"

/* Common */
#define MAX_FLASH_PAGE (MAX_FLASH_BLOCK * PAGES_PER_BLOCK)

typedef enum RETURN_TYPE {
    RET_FAIL,
    RET_SUCCESS,
    RET_ERROR
} RETURN_TYPE;

#define ADDRESS_TYPE_SHIFT 27ULL

typedef enum ADDRESS_TYPE{
    ADDR_NULL,
    ADDR_FLASH,
    ADDR_CELL,
    ADDR_BUFFER,
    ADDR_REMAP
} ADDRESS_TYPE;

typedef enum SPLIT_TYPE {
    SPLIT_REGULAR,
    SPLIT_REMAP,
    SPLIT_LAZY
} SPLIT_TYPE;

typedef enum COMPACTION_TYPE {
    COMPACTION_REGULAR,
    COMPACTION_METADATA,
    COMPACTION_NONE
} COMPACTION_TYPE;

typedef union address {
    uint32_t all;
    struct 
    {
        uint32_t offset : 11;
        uint32_t page   : 6;
        uint32_t block  : 10;
        uint32_t type   : 5;
    };
} address;

/* GRIDTREE Definition */
#define MAX_LEVEL 10
#define GRIDTREE_MAX_SIZE (50L * 1024)
#define GRIDTREE_MAX_COUNT (GRIDTREE_MAX_SIZE/sizeof(Cell))
#define SPLIT_LIST_MAX_COUNT 100
#if (DISTRIBUTION == 0) 
#define CELL_COMPACTION_THRESHOLD 110
#else
#define CELL_COMPACTION_THRESHOLD 80
#endif
#define COMPACTION_LIST_MAX_COUNT 300
#define BM_TIMESTAMP_COMPRESS_SHIFT 3

typedef struct Boundary {
    int32_t x_min;
    int32_t x_max;
    int32_t y_min;
    int32_t y_max;
} Boundary;

typedef struct Cell {
    uint8_t length;
    uint16_t count;
    address ptr;
} Cell;

#define CELL_LEVEL_MAP_MAX_SIZE 1000
typedef struct AddrLevelPair {
    uintptr_t addr;
    uint8_t level;
} AddrLevelPair;

/* Record's Definition */
#define X_MIN -1048576L // -2^20
#define X_MAX 1048576L  //  2^20 
#define Y_MIN 0L
#define Y_MAX 1048576L  //  2^20

#define ACTUAL_X_MIN -200000L
#define ACTUAL_X_MAX 1000000L
#define ACTUAL_Y_MIN 0L
#define ACTUAL_Y_MAX 1000000L

#define RECORD_BUFFER_SIZE (15L * PAGE_SIZE)

typedef struct Record {
    int32_t x;
    int32_t y;
    uint32_t timestamp;
} Record;

/* Buffer's Definition */
// #define FLASH_BUFFER_SIZE (18L * PAGE_SIZE) // DIFFERENTIAL EVALUATION
#define FLASH_BUFFER_SIZE (15L * PAGE_SIZE)

typedef struct BufferEntry {
    Record rec;
    struct BufferEntry *prev;
} BufferEntry;

typedef struct RemapEntry {
    address addr;
    struct RemapEntry *prev;
} RemapEntry;

typedef struct BucketMetadata {
    Cell *cell;
    address rec_head;
    uint16_t size;
    uint16_t min_time;
    uint16_t max_time;
    address prev;
} BucketMetadata;

void init();

Cell *walk(const Record *const rec);

/* Insert */
RETURN_TYPE insert(const Record *const rec);
void flush();

/* Split */
void split(Cell *cell, SPLIT_TYPE type);
void split_regular(address prv);
void split_remap(address prv);
void split_lazy(address prv, Cell *cell, uint8_t level);

/* Compaction */
void compaction(Cell *cell, COMPACTION_TYPE type);
address compaction_regular(Cell *cell);
address compaction_metadata(Cell *cell);

/* Utility */
inline int check_valid_address(const address addr);
inline void *read_records_from_bm(BucketMetadata *const bm_ptr, uint8_t type);
inline BucketMetadata *allocate_bm(Cell * const cell);
inline BufferEntry *allocate_record(const Record *const rec);
inline Cell *allocate_grid(uint8_t level);
inline uint8_t get_cell_level(Cell * const cell);
inline uint8_t intersect(const Record *const a1, const Record *const a2, const Record *const b1, const Record *const b2);
inline void buffer_to_flash(uint8_t page_count, uint8_t *buffer);
void stat_avg_cell_length();
inline uint32_t tree_data_used();

/* Query */
Record *get(const Record *const rec);
uint16_t scan(const Record *const r1, const Record *const r2);

/* Wrappers */
inline RETURN_TYPE INSERT(const Record *const rec);
inline void FLUSH();
inline void SPLIT();
inline void COMPACTION();
inline Record *GET(const Record *const rec);
inline uint16_t SCAN(const Record *const r1, const Record *const r2);
inline void GRIDTREE_STATS();

#endif
