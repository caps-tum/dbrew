
#ifndef TIMER
#define TIMER

#define _POSIX_C_SOURCE 200809L

#include <time.h>

struct JTimer {
  struct timespec timetaken_time;
  struct timespec current_time;
};
typedef struct JTimer JTimer;

void JTimerInit(JTimer*);
void JTimerCont(JTimer*);
void JTimerStop(JTimer*);
double JTimerRead(JTimer*);

#endif
