#ifndef TIMER_H
#define TIMER_H

#include <cstddef>
#include <cstdint>

class Timer
{
public:
  typedef std::size_t TimerType;
  typedef uint64_t NanoSeconds;
  Timer() : total_(0), time_(0) {};
  void start();
  void stop();
  void addTimer(const Timer& other);
  double elapsed();
  void reset();
private:
  NanoSeconds total_;
  NanoSeconds time_;
  NanoSeconds currentTime();
};

#endif
