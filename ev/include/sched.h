#ifndef SCHED_H
#define SCHED_H
#include "config.h"
typedef struct {
    const char *name; void (*fn)(uint32_t now_ms);
    uint32_t period_us, next_us;
    uint32_t runs, misses, skipped, max_exec_us, max_jitter_us; uint64_t sum_exec_us;
} task_t;
#define SCHED_MAX 16
void     sched_init(void);
int      sched_add(const char *name, void (*fn)(uint32_t), uint32_t period_ms, uint32_t offset_ms);
bool     sched_run(void);                 /* one pass; returns true if any task ran */
uint32_t sched_task_count(void);
const task_t *sched_task(uint32_t i);
uint32_t sched_total_misses(void);
uint32_t sched_last_miss_ms(void);
bool     sched_hung_round(void);
#endif
