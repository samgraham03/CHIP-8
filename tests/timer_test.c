#include "../timer.h"

#include <stdio.h>
#include <unistd.h>

// An 8bit 60HZ timer should decrement to zero in approximately 4.25 seconds

int main() {
    timer_60hz_t timer;

    timer_60hz_set(&timer, 0xFF);

    struct timeval start_time = timer.time_stamp;

    while (timer.counter) {
        timer_60hz_decrement(&timer);
        // usleep(1000000);
    }

    struct timeval time_passed = {
        .tv_sec = timer.time_stamp.tv_sec - start_time.tv_sec,
        .tv_usec = timer.time_stamp.tv_usec - start_time.tv_usec
    };
    if (time_passed.tv_usec < 0) {
        time_passed.tv_sec--;
        time_passed.tv_usec += 1000000;
    }

    printf("time passed:\ns:  %li\nms: %li\n", time_passed.tv_sec, time_passed.tv_usec);
    return 0;
}