#include "gridtree.h"
#include "utils.h"
#include "query.h"
#include "debug.h"

extern BREAKPOINT_TYPE breakpoint;
extern Statistics stats;

#pragma PERSISTENT(query_candidate_split) // 0
Record query_candidate_split[SPLIT_LIST_MAX_COUNT] = {0};

#pragma PERSISTENT(query_candidate_compaction) // 1
Record query_candidate_compaction[COMPACTION_LIST_MAX_COUNT] = {0};

#pragma PERSISTENT(query_candidate_head)
uint8_t query_candidate_head[2] = {0};

#pragma PERSISTENT(query_split)
Record query_split[3][200] = {0}; // 0: regular, 1: remap, 2: lazy

#pragma PERSISTENT(query_split_head)
uint8_t query_split_head[3] = {0};

#pragma PERSISTENT(query_compaction)
Record query_compaction[3][200] = {0};

#pragma PERSISTENT(query_compaction_head)
uint8_t query_compaction_head[3] = {0};

#pragma PERSISTENT(query_records)
Record query_records[SCAN_COUNT / 2] = {0};

#pragma PERSISTENT(query_records_head)
uint32_t query_records_head = 0;

inline void collect_query_candidate(const Record *const rec, uint8_t type)
{
    if (type == 0)
        query_candidate_split[query_candidate_head[0]++] = *rec;
    else if (type == 1)
        query_candidate_compaction[query_candidate_head[1]++] = *rec;
}

inline void collect_query_split(uint8_t idx, SPLIT_TYPE type)
{
    query_split[type][query_split_head[type]++] = query_candidate_split[idx];
}

inline void collect_query_compaction(uint8_t idx, COMPACTION_TYPE type)
{
    query_compaction[type][query_compaction_head[type]++] = query_candidate_compaction[idx];
}

inline void collect_scan_query_record()
{
#if DISTRIBUTION == 1
    for (uint8_t i = 0; i < (SCAN_COUNT / 2) * 2 * stats.split_type_cnt[SPLIT_LAZY] / stats.count[PHASE_SPLIT] && query_records_head < (SCAN_COUNT / 2); ++i)
        query_records[query_records_head++] = query_split[SPLIT_LAZY][i];

    for (uint8_t i = 0; i < (SCAN_COUNT / 2) / 2 * stats.split_type_cnt[SPLIT_REGULAR] / stats.count[PHASE_SPLIT] && query_records_head < (SCAN_COUNT / 2); ++i)
        query_records[query_records_head++] = query_split[SPLIT_REGULAR][i];
    
    for (uint8_t i = 0; i < (SCAN_COUNT / 2) / 2 * stats.split_type_cnt[SPLIT_REMAP] / stats.count[PHASE_SPLIT] && query_records_head < (SCAN_COUNT / 2); ++i)
        query_records[query_records_head++] = query_split[SPLIT_REMAP][i];
#endif

#if DISTRIBUTION == 0
    // for (uint8_t i = 0; i < (SCAN_COUNT / 2) * stats.compaction_type_cnt[COMPACTION_REGULAR] / stats.count[PHASE_COMPACTION] && query_records_head < (SCAN_COUNT / 2); ++i)
    //     query_records[SCAN_COUNT/2 - 1 - query_records_head++] = query_compaction[COMPACTION_REGULAR][i];

    // for (uint8_t i = 0; i < (SCAN_COUNT / 2) * stats.compaction_type_cnt[COMPACTION_METADATA] / stats.count[PHASE_COMPACTION] && query_records_head < (SCAN_COUNT / 2); ++i)
    //     query_records[SCAN_COUNT/2 - 1 - query_records_head++] = query_compaction[COMPACTION_METADATA][i];

    // for (uint8_t i = 0; i < (SCAN_COUNT / 2) * stats.compaction_type_cnt[COMPACTION_NONE] / stats.count[PHASE_COMPACTION] && query_records_head < (SCAN_COUNT / 2); ++i)
    //     query_records[SCAN_COUNT/2 - 1 - query_records_head++] = query_compaction[COMPACTION_NONE][i];

    for (uint8_t i = 0; i < (SCAN_COUNT / 2) * stats.compaction_type_cnt[COMPACTION_NONE] / stats.count[PHASE_COMPACTION] && query_records_head < (SCAN_COUNT / 2); ++i)
        query_records[query_records_head++] = query_compaction[COMPACTION_NONE][i];

    for (uint8_t i = 0; i < (SCAN_COUNT / 2) * stats.compaction_type_cnt[COMPACTION_REGULAR] / stats.count[PHASE_COMPACTION] && query_records_head < (SCAN_COUNT / 2); ++i)
        query_records[query_records_head++] = query_compaction[COMPACTION_REGULAR][i];

    for (uint8_t i = 0; i < (SCAN_COUNT / 2) * stats.compaction_type_cnt[COMPACTION_METADATA] / stats.count[PHASE_COMPACTION] && query_records_head < (SCAN_COUNT / 2); ++i)
        query_records[query_records_head++] = query_compaction[COMPACTION_METADATA][i];

#endif

    while (query_records_head < SCAN_COUNT / 2)
    {
        query_records[query_records_head].x = rand_32bits() % (ACTUAL_X_MAX - ACTUAL_X_MIN) + ACTUAL_X_MIN;;
        query_records[query_records_head].y = rand_32bits() % (ACTUAL_X_MAX - ACTUAL_X_MIN) + ACTUAL_X_MIN;;
        query_records_head++;
    }

    for (uint16_t i = 1; i < query_records_head / 2; i+=2)
    {
        Record *r1 = &query_records[i];
        Record *r2 = &query_records[query_records_head - 1 - i];
        Record tmp = *r1;
        *r1 = *r2;
        *r2 = tmp;
    }
}
