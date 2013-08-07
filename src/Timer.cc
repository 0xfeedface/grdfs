#include "Timer.hh"

#ifdef __APPLE__
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

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

void Timer::addTimer(const Timer& other)
{
  total_ += other.total_;
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

void Timer::reset()
{
  total_ = 0;
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

