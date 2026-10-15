// What minmea's header defines as `static inline`, and so has no symbol to link to, and what takes a
// `struct tm` or a `struct timespec`, whose layouts differ between C libraries. Each function calls
// minmea's own code and does nothing else, so the binding gets the answer the C library gives.

#include <stdint.h>
#include <time.h>

#include "minmea.h"

int32_t minmea_sysl_rescale(const struct minmea_float *f, int32_t new_scale)
{
    return minmea_rescale(f, new_scale);
}

float minmea_sysl_tofloat(const struct minmea_float *f)
{
    return minmea_tofloat(f);
}

float minmea_sysl_tocoord(const struct minmea_float *f)
{
    return minmea_tocoord(f);
}

int minmea_sysl_unix_time(const struct minmea_date *date, const struct minmea_time *time_, int64_t *seconds, int32_t *nanoseconds)
{
    struct timespec ts;
    if (minmea_gettime(&ts, date, time_))
        return -1;
    *seconds = (int64_t) ts.tv_sec;
    *nanoseconds = (int32_t) ts.tv_nsec;
    return 0;
}
