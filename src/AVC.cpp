/**
 * @file AVC.c
 * @author Abdo Daood (abdo.daood94@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2023-11-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include "AVC.h"

static const uint8_t *ff_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
    const uint8_t *a = p + 4 - ((intptr_t)p & 3);  // The first address after a=p is 00

    for (end -= 3; p < a && p < end; p++) {        
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    for (end -= 3; p < end; p += 4) {
        uint32_t x = *(const uint32_t*)p;  // Get 4 bytes
        if ((x - 0x01010101) & (~x) & 0x80808080) { // At least one byte in X is 0
            if (p[1] == 0) {
                if (p[0] == 0 && p[2] == 1) // 0 0 1 x
                    return p;
                if (p[2] == 0 && p[3] == 1) // x 0 0 1
                    return p+1;
            }
            if (p[3] == 0) {
                if (p[2] == 0 && p[4] == 1) // x x 0 0 1
                    return p+2;
                if (p[4] == 0 && p[5] == 1) // x x x 0 0 1
                    return p+3;
            }
        }
    }

    for (end += 3; p < end; p++) {  //
        if (p[0] == 0 && p[1] == 0 && p[2] == 1)
            return p;
    }

    return end + 3; // no start code in [p, end], return end.
}

const uint8_t *ff_avc_find_startcode(const uint8_t *p, const uint8_t *end){
    const uint8_t *out= ff_avc_find_startcode_internal(p, end);
    if(p < out && out < end && !out[-1]) out--; // find 0001 in x001
    return out;
}