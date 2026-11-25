#ifndef APP_H
#define APP_H
#include "config.h"
void ev_init(void);      /* boot: drivers, tasks, watchdog */
void ev_step(void);      /* one super-loop iteration       */
#endif
