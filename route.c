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
#include "util.h"

static int compare(const void *a, const void *b) { return memcmp(*(void**)a, *(void**)b, 20); }

void route_init(struct route table[157]) { int i; for(i = 0; i < 157; i++) table[i].offset = table[i].count = 0; }

void route_add(struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], uint32_t addr, uint16_t port)
{
    int i, depth = 0;
    uint8_t metric[20];
    memxor(srcid, dstid, metric, 20);
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
    int depth = 0, dir = 1;
    uint8_t metric[20];
    memxor(srcid, dstid, metric, 20);
    while(1)
    {
        uint8_t n, count = table[depth].count;
        if((count > 0) && (!(metric[depth / 8] & (1 << (7 - depth % 8))) == !dir))
        {
            uint8_t metrics[count][20], *ptrs[count];
            for(n = 0; n < count; n++)
            {
                memxor(table[depth].nodes[n].id, dstid, metrics[n], 20);
                ptrs[n] = metrics[n];
            }

            qsort(ptrs, count, sizeof(void*), compare);
            for(n = 0; (n < count) && (result->count + n < 20); n++)
            {
                int i = (ptrs[n] - metrics[0]) / 20;
                memcpy(&result->nodes[result->count + n], &table[depth].nodes[i], sizeof(struct route_node));
            }
            if((result->count += n) == 20) return;
        }

        if(dir)
            { if(depth == 156) dir = 0; else depth++; }
        else
            { if(depth == 0) return; else depth--; }
    }
}

void route_merge(const struct route_node *source, uint8_t count, const uint8_t dstid[20], struct route *dest)
{
    const uint8_t diff = dest->count - dest->offset;
    struct route_node tmp[diff];
    memcpy(tmp, dest->nodes + dest->offset, diff);

    uint8_t i, n, metric[20], refmetric[20];
    memxor(dest->nodes->id, dstid, refmetric, 20);
    for(n = 0; n < count; n++)
    {
        memxor(source[n].id, dstid, metric, 20);
        if(memcmp(metric, refmetric, 20) >= 0) break;
        memcpy(&dest->nodes[n], &source[n], sizeof(struct route_node));
    }
    for(i = 0; (i < diff) && (i + n < 20); i++)
        memcpy(&dest->nodes[n + i], &tmp[i], sizeof(struct route_node));

    dest->count = i + n;
    dest->offset = 0;
}
