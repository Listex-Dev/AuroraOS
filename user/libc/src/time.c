#include <time.h>
#include <string.h>

time_t time(time_t *tloc)
{
    /* In AuroraOS kernel, PIT tick count is 100 Hz */
    time_t now = 1776518400; /* Approximate epoch timestamp */
    if (tloc) *tloc = now;
    return now;
}

int gettimeofday(struct timeval *tv, void *tz)
{
    (void)tz;
    if (tv) {
        tv->tv_sec = time(NULL);
        tv->tv_usec = 0;
    }
    return 0;
}

clock_t clock(void)
{
    return 0;
}

static struct tm static_tm;

struct tm *localtime(const time_t *timep)
{
    time_t t = timep ? *timep : time(NULL);
    memset(&static_tm, 0, sizeof(static_tm));
    static_tm.tm_sec = t % 60;
    t /= 60;
    static_tm.tm_min = t % 60;
    t /= 60;
    static_tm.tm_hour = t % 24;
    t /= 24;
    static_tm.tm_mday = 1 + (t % 31);
    static_tm.tm_mon = (t / 31) % 12;
    static_tm.tm_year = 126; /* 2026 */
    return &static_tm;
}

struct tm *gmtime(const time_t *timep)
{
    return localtime(timep);
}

time_t mktime(struct tm *tm)
{
    (void)tm;
    return time(NULL);
}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    (void)format; (void)tm;
    const char *str = "2026-10-18 16:00:00";
    size_t len = strlen(str);
    if (len + 1 <= max) {
        strcpy(s, str);
        return len;
    }
    return 0;
}
