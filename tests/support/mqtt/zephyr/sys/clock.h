#pragma once
#include <time.h>
#define SYS_CLOCK_REALTIME 1
static time_t mqtt_test_wall_time;
static inline int sys_clock_gettime(int clock, struct timespec *out) {
	(void)clock; out->tv_sec = mqtt_test_wall_time; out->tv_nsec = 0; return 0;
}
static inline int sys_clock_settime(int clock, const struct timespec *value) {
	(void)clock; mqtt_test_wall_time = value->tv_sec; return 0;
}
