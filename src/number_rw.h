#ifndef __NUMBER_RW__
#define __NUMBER_RW__
#include <stdio.h>
#include "config.h"

// The type of "big number" here will be the same as DocID (essentially, we read DocIDs)
#define RWNumber DocID

// This function can be called several MILLIONS of times during a query. Need to be as fast as possible (and inline of course)
inline RWNumber read_cn(char*& buffer)
{
    register RWNumber number = 0;
    register char ptr;
    do
    {
        ptr = *buffer++;
        number = (number << 7) | (ptr & 0x7F);
    }
    while(ptr & 0x80);
    return number;
}

inline void pass_cn(char*& buffer)
{
    while(*buffer++ & 0x80);
}

inline unsigned char cn_size(RWNumber number)
{
    // FIXME: Need to optimize
    /*
    unsigned char i = 1;
    while(number >> i * 7)
        ++i;
    return i;
    */
    /*
    unsigned char i = 1;
    while(number = number >> 7)
        ++i;
    return i;
    */
    register unsigned char i;
    for(i = sizeof(number) * 8 / 7; i && ! (number >> i * 7); --i);
    return i + 1;
}

inline void write_cn(char*& buffer, RWNumber number)
{
    unsigned char size = cn_size(number) - 1;
    for(unsigned char i = size; i; --i)
        *(buffer + size - i) = ((number >> i*7) & 0x7F) | 0x80;
    *(buffer + size) = number & 0x7F;
    buffer += size + 1;
}

inline void write_cn(FILE* file, RWNumber number)
{
    register char c;
    for(unsigned char i = cn_size(number) - 1; i; --i)
    {
        c = ((number >> i*7) & 0x7F) | 0x80;
        fwrite(&c, sizeof(char), 1, file);
    }
    c = number & 0x7F;
    fwrite(&c, sizeof(char), 1, file);
}

#endif
