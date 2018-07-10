#ifndef _GN_PERF_HPP_
#define _GN_PERF_HPP_ 

inline unsigned int uhash(unsigned int x)
{
    x = ((x >> 16u) ^ x) * 0x45d9f3bu;
    x = ((x >> 16u) ^ x) * 0x45d9f3bu;
    x = (x >> 16) ^ x;
    return x;
}

#endif /* _GN_PERF_HPP_ */
