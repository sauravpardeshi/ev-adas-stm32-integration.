#include "evlog.h"
static evlog_entry_t buf[EVLOG_SIZE]; static uint8_t head, cnt; static uint32_t total;
void evlog_init(void){ head=cnt=0; total=0; }
void evlog_add(uint32_t t, uint8_t code, int32_t arg){
    buf[head].t_ms=t; buf[head].code=code; buf[head].arg=arg;
    head=(uint8_t)((head+1)%EVLOG_SIZE); if(cnt<EVLOG_SIZE) cnt++; total++;
}
uint8_t evlog_count(void){ return cnt; }
uint32_t evlog_total(void){ return total; }
bool evlog_get(uint8_t i, evlog_entry_t *o){
    if (i>=cnt) return false;
    uint8_t start=(uint8_t)((head + EVLOG_SIZE - cnt) % EVLOG_SIZE);
    *o = buf[(start+i)%EVLOG_SIZE]; return true;
}
