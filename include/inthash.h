#ifndef __INTHASH_H_INCLUDED__   // if inthash.h hasn't been included yet...
#define __INTHASH_H_INCLUDED__

#include <cstdint>
#include <string>

uint64_t hash64(uint64_t key, uint64_t mask);
uint64_t kmerhash(const std::string s, uint32_t k);
void test_table();

#endif
