#ifndef _GN_PERF_HPP_
#define _GN_PERF_HPP_ 

inline unsigned int uhash(unsigned int x)
{
    // Wang hash
    x = (x ^ 61) ^ (x >> 16);
    x *= 9u;
    x = x ^ (x >> 4);
    x *= 0x27d4eb2du;
    x = x ^ (x >> 15);
    return x;
}

#endif /* _GN_PERF_HPP_ */
