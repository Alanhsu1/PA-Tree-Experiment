#include "FreeRTOS.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include "utils.h"
#include "my_timer.h"
#include "spi_nand.h"
#include "gridtree.h"
#include "checkpoint.h"
#include "debug.h"
#include "query.h"
#include "config.h"
#include "adc.h"

#include <stdio.h>

#define UNSET_ADDR_TYPE(ADDR) (((uint32_t)ADDR) & ~((uint32_t)0b11111 << ADDRESS_TYPE_SHIFT))
#define SET_ADDR_TYPE(ADDR, TYPE) ((UNSET_ADDR_TYPE(ADDR)) | ((uint32_t)TYPE << ADDRESS_TYPE_SHIFT))

#define INTERSECT_1D(a1, a2, b1, b2) (max((a1), (b1)) <= min((a2), (b2)))
#define INTERSECT_2D(a1x, a1y, a2x, a2y, b1x, b1y, b2x, b2y) ((INTERSECT_1D(a1x, a2x, b1x, b1x)) && (INTERSECT_1D(a1y, a2y, b1y, b2y)))

/*
 * Common
 */

extern BREAKPOINT_TYPE breakpoint;

extern Statistics stats;

extern uint8_t cell_length_array[2000];

extern Snapshot snapshot[SNAPSHOT_SLOT_COUNT];

extern uint8_t snapshot_head;

extern PHASE phase;

extern uint64_t stat_timer_1_start;

extern uint64_t stat_timer_1_end;

extern Record query_candidate_split[SPLIT_LIST_MAX_COUNT];

extern Record query_candidate_compaction[COMPACTION_LIST_MAX_COUNT];

extern uint8_t query_candidate_head[2];

/**
 * Gridtree Declaration
 */

// Actual fanout = 2^fanout
#pragma PERSISTENT(fanout)
const uint8_t fanout[MAX_LEVEL] = {3, 3, 2, 2, 1, 1, 1, 1, 1, 1};

// Actual resolution = 2^resolution_x
#pragma NOINIT(resolution_x)
uint8_t resolution_x[MAX_LEVEL];

// Actual resolution = 2^resolution_y
#pragma NOINIT(resolution_y)
uint8_t resolution_y[MAX_LEVEL];

#pragma PERSISTENT(cells)
Cell cells[GRIDTREE_MAX_COUNT] = {0};

#pragma NOINIT(record_buffer)
uint8_t record_buffer[RECORD_BUFFER_SIZE];

#pragma NOINIT(flash_buffer)
uint8_t flash_buffer[FLASH_BUFFER_SIZE];

#pragma PERSISTENT(scan_buffer)
Record scan_buffer[SCAN_BUFFER_SIZE] = {0};

Record *scan_buffer_head;

#pragma PERSISTENT(rec_head)
BufferEntry *rec_head = (BufferEntry *)record_buffer;

#pragma PERSISTENT(bm_head)
BucketMetadata *bm_head = (BucketMetadata *)&record_buffer[RECORD_BUFFER_SIZE];

#pragma PERSISTENT(cell_head)
Cell *cell_head = cells;

#pragma PERSISTENT(logical_flash_head)
uint16_t logical_flash_head = 0; // page

#pragma PERSISTENT(oldest_flash_block)
uint16_t oldest_flash_block = 0; // logical

#pragma PERSISTENT(cell_level_map)
AddrLevelPair cell_level_map[CELL_LEVEL_MAP_MAX_SIZE] = {0};

#pragma PERSISTENT(cell_level_map_head)
AddrLevelPair *cell_level_map_head = cell_level_map;

#pragma PERSISTENT(split_list)
Cell *split_list[SPLIT_LIST_MAX_COUNT] = {0};

#pragma PERSISTENT(compaction_list)
Cell *compaction_list[COMPACTION_LIST_MAX_COUNT] = {0};

#pragma PERSISTENT(last_voltage_change)
double last_voltage_change = 0;

#pragma PERSISTENT(tree_data_usage)
uint32_t tree_data_usage[INSERT_COUNT/100] = {0};

void init()
{
    init_stats();

    resolution_x[0] = ((uint8_t)log2(X_MAX - X_MIN)) - fanout[0];
    resolution_y[0] = ((uint8_t)log2(Y_MAX - Y_MIN)) - fanout[0];

    for (int i = 1; i < MAX_LEVEL; ++i)
    {
        resolution_x[i] = (resolution_x[i - 1]) - fanout[i];
        resolution_y[i] = (resolution_y[i - 1]) - fanout[i];
    }

    allocate_grid(0);
}

Cell *walk(const Record *const rec)
{
    uint8_t level = 0;

    Boundary bdry;
    bdry.x_min = X_MIN; bdry.x_max = X_MAX; bdry.y_min = Y_MIN; bdry.y_max = Y_MAX;

    uint8_t x_idx = (rec->x - bdry.x_min) >> resolution_x[level];
    uint8_t y_idx = (rec->y - bdry.y_min) >> resolution_y[level];

    Cell *par_cell = cells;
    Cell *cur_cell = par_cell + (x_idx << fanout[level])  + y_idx;

    while (cur_cell->ptr.type == ADDR_CELL)
    {
        bdry.x_min = bdry.x_min + ((uint32_t)x_idx << resolution_x[level]);
        bdry.y_min = bdry.y_min + ((uint32_t)y_idx << resolution_y[level]);

        ++level;

        x_idx = (rec->x - bdry.x_min) >> resolution_x[level];
        y_idx = (rec->y - bdry.y_min) >> resolution_y[level];

        par_cell = (Cell *)UNSET_ADDR_TYPE(cur_cell->ptr.all);
        cur_cell = par_cell + ((uint32_t)x_idx << fanout[level]) + y_idx;
    }

    if ((uintptr_t)cur_cell > (uintptr_t)cell_head)
        SET_BREAKPOINT(BP_ERROR);

    return cur_cell;
}

RETURN_TYPE insert(const Record *const rec)
{
    BufferEntry *be_ptr = allocate_record(rec);

    if (be_ptr == NULL)
        return RET_FAIL;

    Cell *cell = walk(rec);

    BucketMetadata *bm_ptr;

    switch (cell->ptr.type)
    {
    case ADDR_NULL:
    case ADDR_FLASH:
    case ADDR_REMAP:
        bm_ptr = allocate_bm(cell);

        if ((uintptr_t)cell < (uintptr_t)snapshot[snapshot_head ^ 1].cell_head)
            add_undo_log_entry(UNDOLOG_ENTRY_CELL, (uintptr_t)cell);

        cell->ptr.all = SET_ADDR_TYPE(bm_ptr, ADDR_BUFFER);
        break;
    case ADDR_BUFFER:
        bm_ptr = (BucketMetadata *)UNSET_ADDR_TYPE(cell->ptr.all);
        break;
    default:
        SET_BREAKPOINT(BP_ERROR);
        while (1);
    }

    be_ptr->prev = (BufferEntry *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);

    if ((uintptr_t)bm_ptr > (uintptr_t)snapshot[snapshot_head^1].bm_head)
        add_undo_log_entry(UNDOLOG_ENTRY_BM, (uintptr_t)bm_ptr);

    bm_ptr->rec_head.all = SET_ADDR_TYPE(be_ptr, ADDR_BUFFER);
    ++bm_ptr->size; bm_ptr->min_time = min(bm_ptr->min_time, rec->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT); bm_ptr->max_time = max(bm_ptr->max_time, (rec->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT) + 1);

    return RET_SUCCESS;
}

void flush()
{
    BucketMetadata *bm_ptr = (BucketMetadata *)&record_buffer[RECORD_BUFFER_SIZE];
    uintptr_t flash_ptr = (uintptr_t)flash_buffer;
    BufferEntry *be_ptr;
    Record *flash_rec_ptr;
    BucketMetadata *flash_bm_ptr;
    address table_head = {.offset = 0, .page = logical_flash_head % PAGES_PER_BLOCK, .block = logical_flash_head / PAGES_PER_BLOCK, .type = ADDR_NULL};

    if (phase == PHASE_FLUSH) // Regular split reuses this function
    {
        memset(split_list, 0, sizeof(split_list));
        memset(compaction_list, 0, sizeof(compaction_list));
        memset(query_candidate_split, 0, sizeof(query_candidate_split));
        memset(query_candidate_compaction, 0, sizeof(query_candidate_compaction));
        query_candidate_head[0] = 0; 
        query_candidate_head[1] = 0;
    }
    Cell **split_list_head = split_list;
    Cell **compaction_list_head = compaction_list;

    while (bm_ptr != bm_head)
    {  
        --bm_ptr;

        if ((uintptr_t)bm_ptr->cell < (uintptr_t)snapshot[snapshot_head ^ 1].cell_head)
            add_undo_log_entry(UNDOLOG_ENTRY_CELL, (uintptr_t)bm_ptr->cell);

        bm_ptr->cell->ptr.all = SET_ADDR_TYPE(table_head.all, ADDR_FLASH);
        bm_ptr->cell->count += bm_ptr->size; ++bm_ptr->cell->length;

        if (bm_ptr->cell->count > CELL_CAPACITY)
        {
            if (split_list_head != &split_list[SPLIT_LIST_MAX_COUNT])
            {
                *split_list_head = bm_ptr->cell;
                ++split_list_head;

                BufferEntry *tmp = (BufferEntry *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);
                collect_query_candidate((Record *)&tmp->rec, 0); // 0: split candidate
            }
        }
        else // No overlap between compaction candidates and split candidates.
        {
            if (bm_ptr->cell->length > CELL_COMPACTION_THRESHOLD)
            {
                if (compaction_list_head != &compaction_list[COMPACTION_LIST_MAX_COUNT])
                {
                    *compaction_list_head = bm_ptr->cell;
                    ++compaction_list_head;

                    BufferEntry *tmp = (BufferEntry *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);
                    collect_query_candidate((Record *)&tmp->rec, 1); // 1: compaction candidate
                }
            }
        }

        flash_bm_ptr = (BucketMetadata *)flash_ptr;
        flash_bm_ptr->cell = bm_ptr->cell; flash_bm_ptr->rec_head.all = SET_ADDR_TYPE(table_head.all + sizeof(BucketMetadata), ADDR_FLASH); flash_bm_ptr->size = bm_ptr->size; flash_bm_ptr->min_time = bm_ptr->min_time; flash_bm_ptr->max_time = bm_ptr->max_time; flash_bm_ptr->prev = bm_ptr->prev;
        flash_ptr += sizeof(BucketMetadata); table_head.all += sizeof(BucketMetadata);

        flash_rec_ptr = (Record *)flash_ptr;

        be_ptr = (BufferEntry *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);

        for (uint16_t i = 0; i < bm_ptr->size; ++i)
        {
            *flash_rec_ptr = be_ptr->rec;
            be_ptr = be_ptr->prev;
            ++flash_rec_ptr;
        }

        flash_ptr += bm_ptr->size * sizeof(Record); table_head.all += bm_ptr->size * sizeof(Record);
    }

    uint8_t page_count = FLASH_BUFFER_SIZE / PAGE_SIZE;
    buffer_to_flash(page_count, flash_buffer);

    rec_head = (BufferEntry *)record_buffer;
    bm_head = (BucketMetadata *)&record_buffer[RECORD_BUFFER_SIZE];
}

void split(Cell *cell, SPLIT_TYPE type)
{
    uint8_t level;
    address prv;

    level = get_cell_level(cell);
    prv.all = cell->ptr.all;

    if ((uintptr_t)cell < (uintptr_t)snapshot[snapshot_head ^ 1].cell_head)
        add_undo_log_entry(UNDOLOG_ENTRY_CELL, (uintptr_t)cell);

    cell->ptr.all = SET_ADDR_TYPE(allocate_grid(level + 1), ADDR_CELL);
    stats.max_depth = max(stats.max_depth, level + 1);

    switch (type)
    {
    case SPLIT_REGULAR:
        split_regular(prv);
        break;
    case SPLIT_REMAP:
        split_remap(prv);
        break;
    case SPLIT_LAZY:
        split_lazy(prv, (Cell *)UNSET_ADDR_TYPE(cell->ptr.all), level + 1);
        break;
    }

    CHECKPOINT(CHECKPOINT_SPLIT);
}

void split_regular(address prv)
{
    BucketMetadata bm;
    Record *rec_ptr;

    while (check_valid_address(prv) == RET_SUCCESS)
    {
        read_op((prv.block * PAGES_PER_BLOCK + prv.page) % MAX_FLASH_PAGE, prv.offset, (uint8_t *)&bm, sizeof(BucketMetadata));
        if (read_records_from_bm(&bm, prv.type) == NULL)
            break;

        rec_ptr = (Record *)flash_buffer;

        for (uint16_t i = 0; i < bm.size; ++i, ++rec_ptr)
        {
            while (insert(rec_ptr) == RET_FAIL)
            {
                flush();
                read_records_from_bm(&bm, prv.type);
            }
        }
        prv.all = bm.prev.all;
    }
    flush();
}

void split_remap(address prv)
{
    BucketMetadata bm;
    Record *rec_ptr;
    address *remap_ptr;
    RemapEntry *re_ptr = (RemapEntry *)record_buffer;
    BucketMetadata *bm_ptr;
    Cell *cell;
    address addr_head;

    while (check_valid_address(prv) == RET_SUCCESS)
    {
        read_op((prv.block * PAGES_PER_BLOCK + prv.page) % MAX_FLASH_PAGE, prv.offset, (uint8_t *)&bm, sizeof(BucketMetadata));

        if ((remap_ptr = (address *)read_records_from_bm(&bm, prv.type)) == NULL)
            break;

        rec_ptr = (Record *)flash_buffer;
        addr_head.all = UNSET_ADDR_TYPE(bm.rec_head.all);

        for (uint16_t i = 0; i < bm.size && remap_ptr != (address *)&flash_buffer[FLASH_BUFFER_SIZE]; ++i) // Supposedly, it would not overflow the buffer
        {
            re_ptr->addr.all = UNSET_ADDR_TYPE(prv.type == ADDR_REMAP ? remap_ptr->all : addr_head.all);
            addr_head.all += sizeof(Record); ++remap_ptr;

            cell = walk(rec_ptr);
            switch (cell->ptr.type)
            {
            case ADDR_NULL:
                bm_ptr = allocate_bm(cell);
                cell->ptr.all = SET_ADDR_TYPE(bm_ptr, ADDR_BUFFER);
                break;
            case ADDR_BUFFER:
                bm_ptr = (BucketMetadata *)UNSET_ADDR_TYPE(cell->ptr.all);
                break;
            default: // irrelavant records from lazy split
                continue;
            }

            re_ptr->prev = (RemapEntry *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);
            bm_ptr->rec_head.all = SET_ADDR_TYPE(re_ptr, ADDR_BUFFER);
            ++bm_ptr->size; bm_ptr->min_time = min(bm_ptr->min_time, rec_ptr->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT); bm_ptr->max_time = max(bm_ptr->max_time, (rec_ptr->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT) + 1);
            rec_ptr++; re_ptr++;
        }

        prv.all = bm.prev.all;
    }

    // flush
    bm_ptr = (BucketMetadata *)&record_buffer[RECORD_BUFFER_SIZE];
    uintptr_t flash_ptr = (uintptr_t)flash_buffer;
    address *flash_remap_ptr;
    BucketMetadata *flash_bm_ptr;
    address table_head = {.offset = 0, .page = logical_flash_head % PAGES_PER_BLOCK, .block = logical_flash_head / PAGES_PER_BLOCK, .type = ADDR_NULL};

    while (bm_ptr != bm_head)
    {
        --bm_ptr;

        bm_ptr->cell->ptr.all = SET_ADDR_TYPE(table_head.all, ADDR_REMAP);
        bm_ptr->cell->count += bm_ptr->size;

        flash_bm_ptr = (BucketMetadata *)flash_ptr;
        flash_bm_ptr->cell = bm_ptr->cell; flash_bm_ptr->rec_head.all = SET_ADDR_TYPE(table_head.all + sizeof(BucketMetadata), ADDR_REMAP); flash_bm_ptr->size = bm_ptr->size; flash_bm_ptr->min_time = bm_ptr->min_time; flash_bm_ptr->max_time = bm_ptr->max_time; flash_bm_ptr->prev = bm_ptr->prev;
        flash_ptr += sizeof(BucketMetadata); table_head.all += sizeof(BucketMetadata);

        flash_remap_ptr = (address *)flash_ptr;

        re_ptr = (RemapEntry *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);

        for (uint16_t i = 0; i < bm_ptr->size; ++i)
        {
            flash_remap_ptr->all = SET_ADDR_TYPE(re_ptr->addr.all, ADDR_FLASH);
            re_ptr = re_ptr->prev;
            ++flash_remap_ptr;
        }

        flash_ptr += bm_ptr->size * sizeof(address); table_head.all += bm_ptr->size * sizeof(address);
    }

    uint8_t page_count = (flash_ptr - (uintptr_t)flash_buffer + PAGE_SIZE - 1) / PAGE_SIZE;
    buffer_to_flash(page_count, flash_buffer);

    bm_head = (BucketMetadata *)&record_buffer[RECORD_BUFFER_SIZE];
}

void split_lazy(address prv, Cell *cell, uint8_t level)
{
    for (uint8_t i = 0; i < (1L << (2 * fanout[level])); ++i, ++cell)
        cell->ptr.all = prv.all;
}

void compaction(Cell *cell, COMPACTION_TYPE type)
{
    if ((uintptr_t)cell < (uintptr_t)snapshot[snapshot_head ^ 1].cell_head)
        add_undo_log_entry(UNDOLOG_ENTRY_CELL, (uintptr_t)cell);
    
    switch (type)
    {
    case COMPACTION_REGULAR:
        cell->ptr.all = compaction_regular(cell).all;
        cell->length = 1;
        break;
    case COMPACTION_METADATA:
        cell->ptr.all = compaction_metadata(cell).all;
        cell->length = 1;
        break;
    case COMPACTION_NONE:
        break;
    }
}

address compaction_regular(Cell *cell)
{
    BucketMetadata bm;
    address table_head = {.offset = 0, .page = logical_flash_head % PAGES_PER_BLOCK, .block = logical_flash_head / PAGES_PER_BLOCK, .type = ADDR_NULL};
    BucketMetadata compaction_bm = {.cell = cell, .rec_head.all = SET_ADDR_TYPE(table_head.all, ADDR_FLASH), .size = 0, .min_time = UINT16_MAX, .max_time = 0, .prev = NULL};
    uintptr_t buffer_ptr = (uintptr_t)record_buffer;
    address prv = {.all = cell->ptr.all};

    while (check_valid_address(prv) == RET_SUCCESS)
    {
        read_op((prv.block * PAGES_PER_BLOCK + prv.page) % MAX_FLASH_PAGE, prv.offset, (uint8_t *)&bm, sizeof(BucketMetadata));
        if (read_records_from_bm(&bm, prv.type) == NULL)
            break;
        
        if ((uintptr_t)(buffer_ptr + bm.size * sizeof(Record)) > (uintptr_t)&record_buffer[RECORD_BUFFER_SIZE])
            break;
        
        DMA_transfer(flash_buffer, (uint8_t *)buffer_ptr, bm.size * sizeof(Record));
        buffer_ptr += bm.size * sizeof(Record); table_head.all += bm.size * sizeof(Record);
        compaction_bm.size += bm.size; compaction_bm.min_time = min(compaction_bm.min_time, bm.min_time); compaction_bm.max_time = max(compaction_bm.max_time, bm.max_time);

        prv.all = bm.prev.all;
    }

    *(BucketMetadata *)buffer_ptr = compaction_bm;
    buffer_ptr += sizeof(BucketMetadata);

    uint8_t page_count = (buffer_ptr - (uintptr_t)record_buffer + PAGE_SIZE - 1) / PAGE_SIZE;
    buffer_to_flash(page_count, record_buffer);

    table_head.all = SET_ADDR_TYPE(table_head.all, ADDR_FLASH);
    return table_head;
}

address compaction_metadata(Cell *cell)
{
    BucketMetadata bm;
    address table_head = {.offset = 0, .page = logical_flash_head % PAGES_PER_BLOCK, .block = logical_flash_head / PAGES_PER_BLOCK, .type = ADDR_NULL};
    BucketMetadata *bm_ptr = (BucketMetadata *)record_buffer;
    address prv = {.all = cell->ptr.all};
    address ret = {.all = SET_ADDR_TYPE(table_head.all, prv.type)};

    while (check_valid_address(prv) == RET_SUCCESS)
    {
        read_op((prv.block * PAGES_PER_BLOCK + prv.page) % MAX_FLASH_PAGE, prv.offset, (uint8_t *)&bm, sizeof(BucketMetadata));
        bm_ptr->cell = bm.cell; bm_ptr->rec_head.all = bm.rec_head.all; bm_ptr->size = bm.size; bm_ptr->min_time = bm.min_time; bm_ptr->max_time = bm.max_time; bm_ptr->prev.all = ((check_valid_address(bm.prev) == RET_SUCCESS) ? SET_ADDR_TYPE(table_head.all + sizeof(BucketMetadata), bm.prev.type) : 0);
        ++bm_ptr; table_head.all += sizeof(BucketMetadata);
        prv.all = bm.prev.all;
    }

    uint8_t page_count = ((uintptr_t)bm_ptr - (uintptr_t)record_buffer + PAGE_SIZE - 1) / PAGE_SIZE;
    buffer_to_flash(page_count, record_buffer);

    return ret;
}

inline int check_valid_address(const address addr)
{
    switch (addr.type)
    {
        case ADDR_FLASH:
        case ADDR_REMAP:
            return (addr.block >= oldest_flash_block);
        case ADDR_CELL:
            return (UNSET_ADDR_TYPE(addr.all) < (uintptr_t)(&cells[GRIDTREE_MAX_COUNT])) && (UNSET_ADDR_TYPE(addr.all) >= (uintptr_t)cells);
        case ADDR_BUFFER:
            return (UNSET_ADDR_TYPE(addr.all) < (uintptr_t)(&record_buffer[RECORD_BUFFER_SIZE])) && (UNSET_ADDR_TYPE(addr.all) >= (uintptr_t)record_buffer);
        case ADDR_NULL:
            return RET_FAIL;
        default:
            return RET_ERROR;
    }
}

inline void *read_records_from_bm(BucketMetadata *const bm_ptr, uint8_t type)
{
    if (check_valid_address(bm_ptr->rec_head) == RET_FAIL)
        return NULL;

    switch (type)
    {
        case ADDR_FLASH:
            read_op((bm_ptr->rec_head.block * PAGES_PER_BLOCK + bm_ptr->rec_head.page) % MAX_FLASH_PAGE, bm_ptr->rec_head.offset, flash_buffer, bm_ptr->size * sizeof(Record));
            return (void *)flash_buffer;
        case ADDR_BUFFER:
            return (void *)UNSET_ADDR_TYPE(bm_ptr->rec_head.all);
        case ADDR_REMAP:
        {
            uintptr_t remap_start = (uintptr_t)&flash_buffer[FLASH_BUFFER_SIZE] - (bm_ptr->size * sizeof(address));

            if (remap_start < (uintptr_t)(flash_buffer + bm_ptr->size * sizeof(Record)))
                return NULL;

            read_op((bm_ptr->rec_head.block * PAGES_PER_BLOCK + bm_ptr->rec_head.page) % MAX_FLASH_PAGE, bm_ptr->rec_head.offset, (uint8_t *)remap_start, sizeof(address) * bm_ptr->size);
            address *addr_ptr = (address *)remap_start;
            Record *rec_ptr = (Record *)flash_buffer;

            for (uint16_t i = 0; i < bm_ptr->size; ++i)
            {
                if (check_valid_address(*addr_ptr) == RET_SUCCESS)
                {
                    read_op((addr_ptr->block * PAGES_PER_BLOCK + addr_ptr->page) % MAX_FLASH_PAGE, addr_ptr->offset, (uint8_t *)rec_ptr, sizeof(Record));
                    ++rec_ptr;
                }
                else
                    remap_start += sizeof(address);

                ++addr_ptr;
            }

            return (void *)remap_start;
        }
        default:
            return NULL;
    }
}

inline BucketMetadata *allocate_bm(Cell * const cell)
{
    BucketMetadata *ret = --bm_head;

    ret->cell = cell;
    ret->rec_head.all = 0;
    ret->size = 0;
    ret->min_time = UINT16_MAX;
    ret->max_time = 0;
    ret->prev = cell->ptr;

    return ret;
}

inline BufferEntry *allocate_record(const Record *const rec)
{
    if ((uintptr_t)(rec_head + 1) > (uintptr_t)(bm_head - 1))
        return NULL;

    BufferEntry *ret = rec_head;

    ret->rec.x = rec->x; ret->rec.y = rec->y; ret->rec.timestamp = rec->timestamp;
    rec_head++;

    return ret;
}

inline Cell *allocate_grid(uint8_t level)
{
    if (level > MAX_LEVEL)
        SET_BREAKPOINT(BP_ERROR);

    Cell *ret = cell_head;
    cell_head += (1L << (2 * fanout[level]));

    if ((uintptr_t)cell_head > (uintptr_t)(&cells[GRIDTREE_MAX_COUNT]))
        SET_BREAKPOINT(BP_ERROR);

    cell_level_map_head->addr = (uintptr_t)ret; cell_level_map_head->level = level;
    ++cell_level_map_head;

    return ret;
}

inline uint8_t get_cell_level(Cell * const cell)
{
    uintptr_t addr = (uintptr_t)cell;
    AddrLevelPair *alp_ptr = cell_level_map;
    uint8_t ret = 0;

    while (alp_ptr->addr != 0 && alp_ptr != cell_level_map_head)
    {
        if (alp_ptr->addr > addr)
            return ret;
        ret = alp_ptr->level;
        ++alp_ptr;
    }

    return ret;
}

inline uint8_t intersect(const Record *const a1, const Record *const a2, const Record *const b1, const Record *const b2)
{
    return (INTERSECT_1D(a1->timestamp, a2->timestamp, b1->timestamp, b2->timestamp) && INTERSECT_2D(a1->x, a1->y, a2->x, a2->y, b1->x, b1->y, b2->x, b2->y));
}

inline void buffer_to_flash(uint8_t page_count, uint8_t *buffer)
{
    for (uint8_t i = 0; i < page_count; ++i, ++logical_flash_head)
    {
        if (logical_flash_head >= MAX_FLASH_PAGE && logical_flash_head % PAGES_PER_BLOCK == 0)
        {
            erase_op(logical_flash_head % MAX_FLASH_PAGE);
            ++oldest_flash_block;
        }

        write_op(logical_flash_head % MAX_FLASH_PAGE, 0, &buffer[i * PAGE_SIZE], PAGE_SIZE);
    }
}

void stat_avg_cell_length()
{
    Cell *cur_cell = cells;

    while (cur_cell != cell_head)
    {
        if (cur_cell->ptr.type == ADDR_FLASH || cur_cell->ptr.type == ADDR_REMAP)
        {
            cell_length_array[stats.total_valid_cell_count] = cur_cell->length;
            ++stats.total_valid_cell_count;
            stats.total_cell_length += cur_cell->length;
        }
        ++cur_cell;
    }
}

inline uint32_t tree_data_used()
{
    return (cell_head - cells) * sizeof(Cell);
}

Record *get(const Record *const rec)
{
    Cell *cell = walk(rec);
    BucketMetadata bm;
    BufferEntry *be_ptr;
    Record *rec_ptr;
    address prv = {.all = cell->ptr.all};
    address *remap_ptr;

    while (check_valid_address(prv) == RET_SUCCESS)
    {
        switch (prv.type)
        {
        case ADDR_FLASH:
        case ADDR_REMAP:
            read_op((prv.block * PAGES_PER_BLOCK + prv.page) % MAX_FLASH_PAGE, prv.offset, (uint8_t *)&bm, sizeof(BucketMetadata));
            if (INTERSECT_1D(bm.min_time, bm.max_time, (rec->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT), (rec->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT)))
            {
                if ((remap_ptr = (address *)read_records_from_bm(&bm, prv.type)) == NULL)
                    break;

                rec_ptr = (Record *)flash_buffer;

                for (uint16_t i = 0; i < bm.size && remap_ptr != (address *)&flash_buffer[FLASH_BUFFER_SIZE]; ++i)
                {
                    if (rec_ptr->x == rec->x && rec_ptr->y == rec->y && rec_ptr->timestamp == rec->timestamp)
                        return rec_ptr;
                    ++rec_ptr;
                }
            }
            break;
        case ADDR_BUFFER:
            bm = *(BucketMetadata *)UNSET_ADDR_TYPE(prv.all);
            if (INTERSECT_1D(bm.min_time, bm.max_time, (rec->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT), (rec->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT)))
            {
                if ((be_ptr = (BufferEntry *)read_records_from_bm(&bm, prv.type)) == NULL)
                    break;

                for (uint16_t i = 0; i < bm.size; ++i)
                {
                    if (be_ptr->rec.x == rec->x && be_ptr->rec.y == rec->y && be_ptr->rec.timestamp == rec->timestamp)
                        return (Record *)&be_ptr->rec;
                    be_ptr = be_ptr->prev;
                }
            }
            break;
        default:
            goto notfound;
        }
        prv.all = bm.prev.all;
    }

notfound:
    return NULL;
}

static void _scan(const Record *const r1, const Record *const r2, Cell *par_cell, uint8_t level, Boundary *bdry)
{
    if (scan_buffer_head == &scan_buffer[SCAN_BUFFER_SIZE])
        return;

    uint8_t r1_x_idx = (r1->x - bdry->x_min) >> resolution_x[level];
    uint8_t r1_y_idx = (r1->y - bdry->y_min) >> resolution_y[level];
    uint8_t r2_x_idx = (r2->x - bdry->x_min) >> resolution_x[level];
    uint8_t r2_y_idx = (r2->y - bdry->y_min) >> resolution_y[level];

    Cell *cur_cell;
    Boundary cur_bdry = {0};

    for (uint8_t x = r1_x_idx; x <= r2_x_idx; ++x)
    {
        for (uint8_t y = r1_y_idx; y <= r2_y_idx; ++y)
        {
            cur_cell = par_cell + (x << fanout[level]) + y;
            cur_bdry.x_min = bdry->x_min + ((uint32_t)x << resolution_x[level]);
            cur_bdry.y_min = bdry->y_min + ((uint32_t)y << resolution_y[level]);
            
            BucketMetadata bm;
            BufferEntry *be_ptr;
            Record *rec_ptr;
            address prv = {.all = cur_cell->ptr.all};
            address *remap_ptr;

            if (prv.type == ADDR_CELL)
            {
                _scan(r1, r2, (Cell *)UNSET_ADDR_TYPE(prv.all), level + 1, &cur_bdry);
                if (scan_buffer_head == &scan_buffer[SCAN_BUFFER_SIZE])
                    return;
                continue;
            }

            while (check_valid_address(prv) == RET_SUCCESS)
            {
                switch (prv.type)
                {
                case ADDR_FLASH:
                case ADDR_REMAP:
                    read_op((prv.block * PAGES_PER_BLOCK + prv.page) % MAX_FLASH_PAGE, prv.offset, (uint8_t *)&bm, sizeof(BucketMetadata));
                    if (INTERSECT_1D(bm.min_time, bm.max_time, (r1->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT), (r2->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT)))
                    {
                        if ((remap_ptr = (address *)read_records_from_bm(&bm, prv.type)) == NULL)
                            break;
                        rec_ptr = (Record *)flash_buffer;

                        for (uint16_t i = 0; i < bm.size && remap_ptr != (address *)&flash_buffer[FLASH_BUFFER_SIZE]; ++i)
                        {
                            if (intersect(r1, r2, rec_ptr, rec_ptr))
                            {
                                *scan_buffer_head = *rec_ptr; ++stats.scan_found;
                                if (++scan_buffer_head == &scan_buffer[SCAN_BUFFER_SIZE])
                                    return;
                            }

                            ++rec_ptr;
                        }
                    }
                    break;
                case ADDR_BUFFER:
                    bm = *(BucketMetadata *)UNSET_ADDR_TYPE(prv.all);
                    if (INTERSECT_1D(bm.min_time, bm.max_time, (r1->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT), (r2->timestamp >> BM_TIMESTAMP_COMPRESS_SHIFT)))
                    {
                        if ((be_ptr = (BufferEntry *)read_records_from_bm(&bm, prv.type)) == NULL)
                            break;

                        for (uint16_t i = 0; i < bm.size; ++i)
                        {
                            if (intersect(r1, r2, &be_ptr->rec, &be_ptr->rec))
                            {
                                *scan_buffer_head = be_ptr->rec; ++stats.scan_found;
                                if (++scan_buffer_head == &scan_buffer[SCAN_BUFFER_SIZE])
                                    return;
                            }
                            be_ptr = be_ptr->prev;
                        }
                    }
                    break;
                default:
                    break;
                }
                prv.all = bm.prev.all;
            }
        }
    }
}

uint16_t scan(const Record *const r1, const Record *const r2)
{
    Boundary bdry;
    bdry.x_min = X_MIN; bdry.x_max = X_MAX; bdry.y_min = Y_MIN; bdry.y_max = Y_MAX;

    scan_buffer_head = scan_buffer;
    memset(scan_buffer, 0, sizeof(scan_buffer));

    _scan(r1, r2, cells, 0, &bdry);

    return ((uintptr_t)scan_buffer_head - (uintptr_t)scan_buffer) / sizeof(Record);
}

/*
 * Wrappers
 */
inline RETURN_TYPE INSERT(const Record *const rec)
{
#ifdef STAT_TIME_MIN_MAX
    high_res_timer_start();
    stat_timer_1_start = get_current_tick(HIGH_RES_CLK);
#endif

    RETURN_TYPE ret;

    phase = PHASE_INSERT;
    
    if (rec->x < X_MIN || rec->x >= X_MAX || rec->y < Y_MIN || rec->y >= Y_MAX)
        SET_BREAKPOINT(BP_ERROR);

    ++stats.count[PHASE_INSERT];

    ret = insert(rec);

#ifdef STAT_TIME_MIN_MAX
    stat_timer_1_end = get_current_tick(HIGH_RES_CLK);
    if (stat_timer_1_end > stat_timer_1_start)
    {
        uint64_t elapsed_ticks = get_elapsed_ticks(stat_timer_1_start, stat_timer_1_end);
        stats.elapsed_time[PHASE_INSERT] += elapsed_ticks;
        stats.min_time[PHASE_INSERT] = min(stats.min_time[PHASE_INSERT], elapsed_ticks);
        stats.max_time[PHASE_INSERT] = max(stats.max_time[PHASE_INSERT], elapsed_ticks);
    }
#endif

    return ret;
}

inline void FLUSH()
{
    phase = PHASE_FLUSH;

    ++stats.count[PHASE_FLUSH];

    stat_timer_1_start = get_current_tick(LOW_RES_CLK);
    flush();
    stat_timer_1_end = get_current_tick(LOW_RES_CLK);

    stats.elapsed_time[PHASE_FLUSH] += get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK);
    stats.min_time[PHASE_FLUSH] = min(stats.min_time[PHASE_FLUSH], get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK));
    stats.max_time[PHASE_FLUSH] = max(stats.max_time[PHASE_FLUSH], get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK));

    CHECKPOINT(CHECKPOINT_FLUSH);
}

static inline SPLIT_TYPE split_type()
{
#if EXPERIMENT == EXP_DIFF_EVAL_SPLIT
    return SPLIT_LAZY;
#endif
#if EXPERIMENT == EXP_DIFF_EVAL_COMPACTION
    return SPLIT_REGULAR;
#endif
#if (EXPERIMENT == EXP_POWER_EVENT) || (EXPERIMENT == EXP_TEST)
    uint8_t rn = rand() % 100;
    return (rn < 0 ? SPLIT_REGULAR : (rn < 0 ? SPLIT_REMAP : SPLIT_LAZY));
#endif
#if EXPERIMENT == EXP_LOGGING
    return SPLIT_LAZY;
#endif
#if EXPERIMENT == EXP_ADAPTIVE
    float cur_voltage = get_voltage();

    if (cur_voltage >= AMPLE_ENERGY_VOLTAGE_THRESHOLD)
        return SPLIT_REGULAR;
    else if (last_voltage_change <= VOLTAGE_DROP_THRESHOLD)
        return SPLIT_REMAP;
    else
        return SPLIT_LAZY;
    
#endif
#if EXPERIMENT == EXP_CELL_STATS
    return SPLIT_REGULAR;
#endif
}

inline void SPLIT()
{
    Cell **split_list_ptr = split_list;
    SPLIT_TYPE type;
    last_voltage_change = 0;
    uint8_t idx = 0;

#ifdef STAT_ENERGY_MIN_MAX
        double energy_before = stats.flash_page_write_cnt[PHASE_SPLIT] * FLASH_PAGE_WRITE_ENERGY + stats.flash_spi_transmit_cnt[PHASE_SPLIT] * SPI_ENERGY + stats.flash_page_read_cnt[PHASE_SPLIT] * FLASH_PAGE_READ_ENERGY + stats.flash_spi_receive_cnt[PHASE_SPLIT] * SPI_ENERGY;
#endif

    stat_timer_1_start = get_current_tick(LOW_RES_CLK);
    while (*split_list_ptr != NULL)
    {
        phase = PHASE_SPLIT;
        type = split_type();

        collect_query_split(idx++, type);

        double voltage_before = get_voltage();
        split(*split_list_ptr, type);
        double voltage_after = get_voltage();
        last_voltage_change = voltage_after - voltage_before;

        ++split_list_ptr;

        ++stats.split_type_cnt[type]; ++stats.count[PHASE_SPLIT];

        CHECKPOINT(CHECKPOINT_SPLIT);
    }
    stat_timer_1_end = get_current_tick(LOW_RES_CLK);

    stats.elapsed_time[PHASE_SPLIT] += get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK);

    uint16_t split_count = ((uintptr_t)split_list_ptr - (uintptr_t)split_list) / sizeof(Cell *);
    if (split_count == 0)
        return;

#ifdef STAT_ENERGY_MIN_MAX
    double energy_after = stats.flash_page_write_cnt[PHASE_SPLIT] * FLASH_PAGE_WRITE_ENERGY + stats.flash_spi_transmit_cnt[PHASE_SPLIT] * SPI_ENERGY + stats.flash_page_read_cnt[PHASE_SPLIT] * FLASH_PAGE_READ_ENERGY + stats.flash_spi_receive_cnt[PHASE_SPLIT] * SPI_ENERGY;
    stats.min_energy[PHASE_SPLIT] = min(stats.min_energy[PHASE_SPLIT], (energy_after - energy_before) / split_count);
    stats.max_energy[PHASE_SPLIT] = max(stats.max_energy[PHASE_SPLIT], (energy_after - energy_before) / split_count);
#endif

#ifdef STAT_TIME_MIN_MAX
    stats.min_time[PHASE_SPLIT] = min(stats.min_time[PHASE_SPLIT], get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK) / split_count);
    stats.max_time[PHASE_SPLIT] = max(stats.max_time[PHASE_SPLIT], get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK) / split_count);
#endif
}

static inline COMPACTION_TYPE compaction_type()
{
#if EXPERIMENT == EXP_DIFF_EVAL_SPLIT
    return COMPACTION_REGULAR;
#endif
#if EXPERIMENT == EXP_DIFF_EVAL_COMPACTION
    return COMPACTION_METADATA;
#endif
#if (EXPERIMENT == EXP_POWER_EVENT) || (EXPERIMENT == EXP_TEST)
    uint8_t rn = rand() % 100;
    return (rn < 0 ? COMPACTION_REGULAR : (rn < 0 ? COMPACTION_METADATA : COMPACTION_NONE));
#endif
#if EXPERIMENT == EXP_LOGGING
    return COMPACTION_NONE;
#endif
#if EXPERIMENT == EXP_ADAPTIVE 
    float cur_voltage = get_voltage();

    if (cur_voltage >= AMPLE_ENERGY_VOLTAGE_THRESHOLD)
        return COMPACTION_REGULAR;
    else if (last_voltage_change <= VOLTAGE_DROP_THRESHOLD)
        return COMPACTION_METADATA;
    else
        return COMPACTION_NONE;

#endif
#if EXPERIMENT == EXP_CELL_STATS
    return COMPACTION_NONE;
#endif
}

inline void COMPACTION()
{
    Cell **compaction_list_ptr = compaction_list;
    COMPACTION_TYPE type;
    last_voltage_change = 0;
    uint8_t idx = 0;

#ifdef STAT_ENERGY_MIN_MAX
        double energy_before = stats.flash_page_write_cnt[PHASE_COMPACTION] * FLASH_PAGE_WRITE_ENERGY + stats.flash_spi_transmit_cnt[PHASE_COMPACTION] * SPI_ENERGY + stats.flash_page_read_cnt[PHASE_COMPACTION] * FLASH_PAGE_READ_ENERGY + stats.flash_spi_receive_cnt[PHASE_COMPACTION] * SPI_ENERGY;
#endif
    
    stat_timer_1_start = get_current_tick(LOW_RES_CLK);
    while (*compaction_list_ptr != NULL)
    {
        phase = PHASE_COMPACTION;
        type = compaction_type();

        collect_query_compaction(idx++, type);

        double voltage_before = get_voltage();
        compaction(*compaction_list_ptr, type);
        double voltage_after = get_voltage();
        last_voltage_change = voltage_after - voltage_before;

        ++compaction_list_ptr;

        ++stats.compaction_type_cnt[type]; ++stats.count[PHASE_COMPACTION]; 

        CHECKPOINT(CHECKPOINT_COMPACTION);
    }
    stat_timer_1_end = get_current_tick(LOW_RES_CLK);

    stats.elapsed_time[PHASE_COMPACTION] += get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK);

    uint16_t compaction_count = ((uintptr_t)compaction_list_ptr - (uintptr_t)compaction_list) / sizeof(Cell *);
    if (compaction_count == 0)
        return;

#ifdef STAT_ENERGY_MIN_MAX
    double energy_after = stats.flash_page_write_cnt[PHASE_COMPACTION] * FLASH_PAGE_WRITE_ENERGY + stats.flash_spi_transmit_cnt[PHASE_COMPACTION] * SPI_ENERGY + stats.flash_page_read_cnt[PHASE_COMPACTION] * FLASH_PAGE_READ_ENERGY + stats.flash_spi_receive_cnt[PHASE_COMPACTION] * SPI_ENERGY;
    stats.min_energy[PHASE_COMPACTION] = min(stats.min_energy[PHASE_COMPACTION], (energy_after - energy_before) / compaction_count);
    stats.max_energy[PHASE_COMPACTION] = max(stats.max_energy[PHASE_COMPACTION], (energy_after - energy_before) / compaction_count);
#endif

#ifdef STAT_TIME_MIN_MAX
    stats.min_time[PHASE_COMPACTION] = min(stats.min_time[PHASE_COMPACTION], get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK) / compaction_count);
    stats.max_time[PHASE_COMPACTION] = max(stats.max_time[PHASE_COMPACTION], get_elapsed_time(stat_timer_1_start, stat_timer_1_end, LOW_RES_CLK) / compaction_count);
#endif
}

inline Record *GET(const Record *const rec)
{
    phase = PHASE_GET;

    ++stats.count[PHASE_GET];

    if (rec->x < X_MIN || rec->x >= X_MAX || rec->y < Y_MIN || rec->y >= Y_MAX)
        return NULL;

    return get(rec);
}

inline uint16_t SCAN(const Record *const r1, const Record *const r2)
{
    phase = PHASE_SCAN;

    ++stats.count[PHASE_SCAN];

#ifdef STAT_TIME_MIN_MAX
    uint64_t start_tick = get_current_tick(LOW_RES_CLK);
#endif

#ifdef STAT_ENERGY_MIN_MAX
    double energy_before = stats.flash_page_write_cnt[PHASE_SCAN] * FLASH_PAGE_WRITE_ENERGY + stats.flash_spi_transmit_cnt[PHASE_SCAN] * SPI_ENERGY + stats.flash_page_read_cnt[PHASE_SCAN] * FLASH_PAGE_READ_ENERGY + stats.flash_spi_receive_cnt[PHASE_SCAN] * SPI_ENERGY; 
#endif

    uint16_t ret = scan(r1, r2);

#ifdef STAT_TIME_MIN_MAX
    uint64_t end_tick = get_current_tick(LOW_RES_CLK);
    stats.min_time[PHASE_SCAN] = min(stats.min_time[PHASE_SCAN], get_elapsed_time(start_tick, end_tick, LOW_RES_CLK));
    stats.max_time[PHASE_SCAN] = max(stats.max_time[PHASE_SCAN], get_elapsed_time(start_tick, end_tick, LOW_RES_CLK));
#endif

#ifdef STAT_ENERGY_MIN_MAX
    double energy_after = stats.flash_page_write_cnt[PHASE_SCAN] * FLASH_PAGE_WRITE_ENERGY + stats.flash_spi_transmit_cnt[PHASE_SCAN] * SPI_ENERGY + stats.flash_page_read_cnt[PHASE_SCAN] * FLASH_PAGE_READ_ENERGY + stats.flash_spi_receive_cnt[PHASE_SCAN] * SPI_ENERGY; 
    stats.min_energy[PHASE_SCAN] = min(stats.min_energy[PHASE_SCAN], (energy_after - energy_before));
    stats.max_energy[PHASE_SCAN] = max(stats.max_energy[PHASE_SCAN], (energy_after - energy_before));
#endif

    return ret;
}

inline void GRIDTREE_STATS()
{
    stat_avg_cell_length();
}
