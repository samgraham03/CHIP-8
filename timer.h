#ifndef TIMER_H
#define TIMER_H

#include <sys/time.h>
#include <stdint.h>

typedef struct {
    uint8_t counter; // todo: this could be a pointer to an address in memory
    long us_passed;
    struct timeval time_stamp;
} timer_60hz_t;

/* Start a 60HZ timer */
void timer_60hz_set(timer_60hz_t* timer, const uint8_t value);

/* Decrement a timer @60HZ proportional to how much time has passed since last decrement / start */
void timer_60hz_decrement(timer_60hz_t* timer);

#endif