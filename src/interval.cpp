#include <iostream>
#include <cassert>
#include "interval.h"

using namespace std;

Interval::Interval(uint32_t s, uint32_t e): start(s), end(e) 
{
    assert(end >= start); // not a real interval ;
    length = end - start; // intervals need to be exclusive of end point so that empty strings can be represented
}

ostream& operator<< (ostream & out, Interval const& i) {
    out << "[" << i.start << ", " << i.end << ")";
    return out ;
}

bool Interval::operator == (const Interval& y) const {
    return (start == y.start and end == y.end);
}

bool Interval::operator != (const Interval& y) const {
    return !(start == y.start and end == y.end);
}

bool Interval::operator < ( const Interval& y) const
{
    if (start < y.start) { return true; }
    if (start > y.start) { return false; }
    if ( end < y.end ) { return true; }
    if ( end > y.end ) { return false; }
    return false;
}
