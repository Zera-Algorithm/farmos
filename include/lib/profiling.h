#ifndef _PROFILING_H
#define _PROFILING_H
#include <dev/timer.h>

void profiling_init();
void profiling_report();
void profiling_end(const char *file, const char *func, u64 begin);

#define PROFILING_DEBUG

#ifdef PROFILING_DEBUG
#define PROFILING_START u64 _profiling_start = time_rtc_us();
#define PROFILING_END profiling_end(__FILE__, __func__, _profiling_start);
#define PROFILING_END_WITH_NAME(name) profiling_end(__FILE__, name, _profiling_start);
#else
#define PROFILING_START {}
#define PROFILING_END {}
#define PROFILING_END_WITH_NAME(name) {}
#endif

#endif
