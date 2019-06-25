#ifndef __MINIMIZER_H_INCLUDED__   // if minimizer.h hasn't been included yet...
#define __MINIMIZER_H_INCLUDED__

#include <ostream>
#include <cstdint>
#include "interval.h"

/**
 * Represents a minimizer from a read or sequence (not from a graph, as MiniRecord)
 */
struct Minimizer {
    uint64_t canonical_kmer_hash;
    Interval pos; //position of the kmer in the read
    bool strand; //strand of the kmer

    Minimizer() {};

    Minimizer(uint64_t, uint32_t, uint32_t, bool);

    ~Minimizer();

    bool operator<(const Minimizer &y) const;

    bool operator==(const Minimizer &y) const;

    friend std::ostream &operator<<(std::ostream &out, const Minimizer &m);
};

#endif
