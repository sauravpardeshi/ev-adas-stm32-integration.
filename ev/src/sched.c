/* sched.c - cooperative, table-driven scheduler with per-task timing statistics */
#include "sched.h"
#include "hal.h"
#include "vehicle.h"
#include "fault.h"
#include <string.h>

static task_t tasks[SCHED_MAX]; static uint32_t n; static uint32_t total_miss, last_miss_ms;
static bool slow_round;

void sched_init(void){ memset(tasks, 0, sizeof tasks); n = 0; total_miss = 0; last_miss_ms = 0; slow_round = false; }
int sched_add(const char *name, void (*fn)(uint32_t), uint32_t period_ms, uint32_t offset_ms){
    if (n >= SCHED_MAX) return -1;
    tasks[n].name = name; tasks[n].fn = fn; tasks[n].period_us = period_ms * 1000u;
    tasks[n].next_us = hal_micros() + offset_ms * 1000u; return (int)n++;
}
uint32_t sched_task_count(void){ return n; }
const task_t *sched_task(uint32_t i){ return i < n ? &tasks[i] : 0; }
uint32_t sched_total_misses(void){ return total_miss; }
uint32_t sched_last_miss_ms(void){ return last_miss_ms; }
bool sched_hung_round(void){ return slow_round; }

bool sched_run(void){
    bool ran = false; slow_round = false;
    for (uint32_t i = 0; i < n; i++) {
        task_t *t = &tasks[i]; uint32_t now = hal_micros();
        if ((int32_t)(now - t->next_us) < 0) continue;
        uint32_t jitter = now - t->next_us;
        t->fn(hal_millis());
        uint32_t end = hal_micros(), exec = end - now;
        t->runs++; t->sum_exec_us += exec;
        if (exec > t->max_exec_us) t->max_exec_us = exec;
        if (jitter > t->max_jitter_us) t->max_jitter_us = jitter;
        if (exec > TASK_HANG_LIMIT_US) slow_round = true;
        if ((uint32_t)(end - t->next_us) > t->period_us) {          /* finished after its deadline (= next release) */
            t->misses++; total_miss++; last_miss_ms = hal_millis(); fault_set(F_SCHED, last_miss_ms);
        }
        t->next_us += t->period_us;
        while ((int32_t)(end - t->next_us) >= (int32_t)t->period_us) { t->next_us += t->period_us; t->skipped++; }
        ran = true;
    }
    return ran;
}
