#ifndef __PATH_H_INCLUDED__   // if path.h hasn't been included yet...
#define __PATH_H_INCLUDED__

#include <deque>
#include <cstdint> //or <stdint.h>
#include <ostream>
#include "interval.h"

using namespace std;

struct Interval;

class Path {
  public:
    deque<Interval> path;
    uint32_t length;
    uint32_t start;
    uint32_t end;

    Path();
    ~Path();
    void add_start_interval(Interval);
    void add_end_interval(Interval);
    void initialize(deque<Interval>);
  friend ostream& operator<< (ostream& out, const Path& p); 
};

#endif
