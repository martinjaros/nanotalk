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

static int memcmp20(const void *a, const void *b) { return memcmp(a, b, 20); }

void route_init(struct route table[157])
{
    int i;
    for(i = 0; i < 157; i++) table[i].count = 0;
}

void route_add(struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], uint32_t addr, uint16_t port)
{
    uint8_t metric[20];
    int i, depth = 0;
    for(i = 0; i < 20; i++) metric[i] = srcid[i] ^ dstid[i];
    while((metric[depth / 8] & (1 << depth % 8)) == 0) if(++depth == 156) break;
    if(table[depth].count == 20)
    {
        memmove(&table[depth].nodes[0], &table[depth].nodes[1], sizeof(struct route_node) * 19);
        table[depth].count--;
    }

    memcpy(table[depth].nodes[table[depth].count].id, dstid, 20);
    table[depth].nodes[table[depth].count].addr = addr;
    table[depth].nodes[table[depth].count].port = port;
    table[depth].count++;
}

void route_lookup(const struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], struct route *result)
{
    result->count = 0;
    uint8_t metric[20];
    int i, depth = 0, dir = 1;
    for(i = 0; i < 20; i++) metric[i] = srcid[i] ^ dstid[i];
    do
    {
        if(dir)
            while((metric[depth / 8] & (1 << depth % 8)) == 0) if(++depth == 156) { dir = 0; break; }
        else
            while((metric[depth / 8] & (1 << depth % 8)) == 1) if(--depth == 0) return;

        uint8_t n, count = table[depth].count;
        if(count)
        {
            uint8_t metrics[20][count], *ptrs[count];
            for(n = 0; n < count; n++) for(i = 0; i < 20; i++)
            {
                metrics[i][n] = table[depth].nodes[n].id[i] ^ dstid[i];
                ptrs[n] = metrics[n];
            }
            qsort(ptrs, count, sizeof(void*), memcmp20);
            for(n = 0; (n < count) && (result->count + n < 20); n++)
            {
                i = (ptrs[n] - metrics[0]) / 20;
                memcpy(result->nodes[result->count + n].id, table[depth].nodes[i].id, 20);
                result->nodes[result->count + n].addr = table[depth].nodes[i].addr;
                result->nodes[result->count + n].port = table[depth].nodes[i].port;
            }
            result->count += n;
        }
    }
    while(result->count < 20);
}

void route_merge(const struct route *a, const struct route *b, const uint8_t dstid[20], struct route *result)
{
    int i, n;
    uint8_t ametrics[20][a->count], bmetrics[20][b->count];
    for(n = 0; n < a->count; n++) for(i = 0; i < 20; i++) ametrics[i][n] = a->nodes[n].id[i] ^ dstid[i];
    for(n = 0; n < b->count; n++) for(i = 0; i < 20; i++) bmetrics[i][n] = b->nodes[n].id[i] ^ dstid[i];

    result->count = 0;
    int aindex = 0, bindex = 0;
    do
    {
        if(aindex == a->count) { if(bindex == b->count) break; else goto broute; } else if(bindex == b->count) goto aroute;
        if(memcmp(ametrics[aindex], bmetrics[bindex], 20) == 1) goto broute;

        aroute:
        memcpy(result->nodes[result->count].id, a->nodes[aindex].id, 20);
        result->nodes[result->count].addr = a->nodes[aindex].addr;
        result->nodes[result->count].port = a->nodes[aindex].port;
        aindex++; continue;

        broute:
        memcpy(result->nodes[result->count].id, b->nodes[bindex].id, 20);
        result->nodes[result->count].addr = b->nodes[bindex].addr;
        result->nodes[result->count].port = b->nodes[bindex].port;
        bindex++; continue;
    }
    while(++result->count < 20);
}
