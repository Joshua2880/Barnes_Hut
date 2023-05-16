#pragma once
#include <cstdint>

union double_bit_t
{
    double d;
    struct
    {
        uint64_t sign : 1;
        uint64_t exponent : 11;
        uint64_t mantissa : 52;
    };
};
