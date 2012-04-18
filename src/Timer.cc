#ifdef __APPLE__
#include <mach/mach_time.h>
#else
#include <sys/time.h>
#include <time.h>
#endif

#include "Timer.h"

void Timer::start()
{
  time_ = currentTime();
}

void Timer::stop()
{
  NanoSeconds t = currentTime();
  total_ += t - time_;
  time_ = 0;
}

double Timer::elapsed()
{
#ifdef __APPLE__
  struct mach_timebase_info info;
  mach_timebase_info(&info);
  return static_cast<double>(1e-6 * total_ * info.numer / info.denom);
#else
  return static_cast<double>(1e-6 * total_);
#endif
}

// private
Timer::NanoSeconds Timer::currentTime()
{
  NanoSeconds time;
#ifdef __APPLE__
  time = mach_absolute_time();
#else
  struct timespec s;
  clock_gettime(CLOCK_REALTIME, &s);
  time = static_cast<NanoSeconds>(s.tv_sec) * 1e9 + static_cast<NanoSeconds>(s.tv_nsec);
#endif
  return time;
}

