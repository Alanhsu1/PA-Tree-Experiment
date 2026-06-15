#include "gridtree.h"

inline void collect_query_candidate(const Record *const rec, uint8_t type);
inline void collect_query_split(uint8_t idx, SPLIT_TYPE type);
inline void collect_query_compaction(uint8_t idx, COMPACTION_TYPE type);
inline void collect_scan_query_record();
