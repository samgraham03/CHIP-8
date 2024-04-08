#ifndef TIMER_H
#define TIMER_H

#include <sys/time.h>
#include <stdint.h>

typedef struct {
    uint8_t counter;
    long us_passed;
    struct timeval time_stamp;
} timer_60hz_t;

/* Start a 60HZ timer */
void timer_60hz_start(timer_60hz_t* timer);

/* Decrement a timer @60HZ proportional to how much time has passed since last decrement / start */
void timer_60hz_decrement(timer_60hz_t* timer);

#endif