#include "timer.h"

#include <unistd.h>

void timer_60hz_set(timer_60hz_t* timer, const uint8_t value) {
    timer->counter = value;
    timer->us_passed = 0;
    (void) gettimeofday(&timer->time_stamp, NULL);
}

void timer_60hz_decrement(timer_60hz_t* timer) {
    if (timer->counter == 0) {
        return;
    }

    static const int MICROSECONDS_PER_SECOND    = 1000000;
    static const int MICROSECONDS_PER_60HZ      = MICROSECONDS_PER_SECOND/60;

    struct timeval current_time;
    (void) gettimeofday(&current_time, NULL);

    struct timeval time_passed = {
        .tv_sec = current_time.tv_sec - timer->time_stamp.tv_sec,
        .tv_usec = current_time.tv_usec - timer->time_stamp.tv_usec
    };

    // Get total time passed in microseconds (since last interaction)
    timer->us_passed += MICROSECONDS_PER_SECOND*time_passed.tv_sec + time_passed.tv_usec;

    // Decrement @60HZ proportional to how much time has passed
    while (timer->us_passed >= MICROSECONDS_PER_60HZ && timer->counter > 0) {
        timer->counter--;
        timer->us_passed -= MICROSECONDS_PER_60HZ;
    }

    timer->time_stamp = current_time;
}