/*
 * Copyright (C) 2015 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <string.h>
#include <stdlib.h>

#include "route.h"

static int compare(const void *a, const void *b) { return memcmp(*(void**)a, *(void**)b, 20); }

void route_init(struct route table[157]) { int i; for(i = 0; i < 157; i++) table[i].offset = table[i].count = 0; }

void route_add(struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], uint32_t addr, uint16_t port)
{
    uint8_t metric[20];
    int i, depth = 0;
    for(i = 0; i < 20; i++) metric[i] = srcid[i] ^ dstid[i];
    while((metric[depth / 8] & (1 << (7 - depth % 8))) == 0) if(++depth == 156) break;
    for(i = 0; i < table[depth].count; i++) if(memcmp(table[depth].nodes[i].id, dstid, 20) == 0) goto match;
    i = table[depth].offset;
    if(table[depth].count < 20) table[depth].count++;
    if(++table[depth].offset == 20) table[depth].offset = 0;

match:
    memcpy(table[depth].nodes[i].id, dstid, 20);
    table[depth].nodes[i].addr = addr;
    table[depth].nodes[i].port = port;
}

void route_lookup(const struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], struct route *result)
{
    result->offset = result->count = 0;
    uint8_t metric[20];
    int i, depth = 0, dir = 1;
    for(i = 0; i < 20; i++) metric[i] = srcid[i] ^ dstid[i];
    while(1)
    {
        uint8_t n, count = table[depth].count;
        if((count > 0) && (!(metric[depth / 8] & (1 << (7 - depth % 8))) == !dir))
        {
            uint8_t metrics[count][20], *ptrs[count];
            for(n = 0; n < count; n++) for(i = 0; i < 20; i++)
            {
                metrics[n][i] = table[depth].nodes[n].id[i] ^ dstid[i];
                ptrs[n] = metrics[n];
            }

            qsort(ptrs, count, sizeof(void*), compare);
            for(n = 0; (n < count) && (result->count + n < 20); n++)
            {
                i = (ptrs[n] - metrics[0]) / 20;
                memcpy(result->nodes[result->count + n].id, table[depth].nodes[i].id, 20);
                result->nodes[result->count + n].addr = table[depth].nodes[i].addr;
                result->nodes[result->count + n].port = table[depth].nodes[i].port;
            }
            if((result->count += n) == 20) return;
        }

        if(dir)
            { if(depth == 156) dir = 0; else depth++; }
        else
            { if(depth == 0) return; else depth--; }
    }
}

void route_merge(const struct route *a, const struct route *b, const uint8_t dstid[20], struct route *result)
{
    int i, n;
    uint8_t ametrics[a->count][20], bmetrics[b->count][20];
    for(n = 0; n < a->count; n++) for(i = 0; i < 20; i++) ametrics[n][i] = a->nodes[n].id[i] ^ dstid[i];
    for(n = 0; n < b->count; n++) for(i = 0; i < 20; i++) bmetrics[n][i] = b->nodes[n].id[i] ^ dstid[i];

    result->offset = result->count = 0;
    int aindex = 0, bindex = 0, count = a->count + b->count;
    while(count-- > 0)
    {
        int res = (aindex == a->count) ? 1 : (bindex == b->count) ? -1 : memcmp(ametrics[aindex], bmetrics[bindex], 20);
        if(res < 0)
        {
            memcpy(result->nodes[result->count].id, a->nodes[aindex].id, 20);
            result->nodes[result->count].addr = a->nodes[aindex].addr;
            result->nodes[result->count].port = a->nodes[aindex].port;
            aindex++; result->count++;
        }
        else if(res > 0)
        {
            memcpy(result->nodes[result->count].id, b->nodes[bindex].id, 20);
            result->nodes[result->count].addr = b->nodes[bindex].addr;
            result->nodes[result->count].port = b->nodes[bindex].port;
            bindex++; result->count++;
        }
        else aindex++;
    }
}
