#ifndef __INTERVAL_H_INCLUDED__   // if interval.h hasn't been included yet...
#define __INTERVAL_H_INCLUDED__

#include <cstdint>
#include <ostream>

struct Interval {
    uint32_t start;
    uint32_t end; //in pilot, longest prg was 208,562 characters long
    uint32_t length;

    Interval(uint32_t=0, uint32_t=0);
    friend std::ostream& operator<< (std::ostream& out, const Interval& i); 
    bool operator == (const Interval& y) const;
    bool operator != (const Interval& y) const;
    bool operator < (const Interval& y) const;
};
#endif
