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

void route_merge(const struct route_node *source, uint8_t count, const uint8_t srcid[20], const uint8_t dstid[20], struct route *dest)
{
    const uint8_t diff = dest->count - dest->offset, total = count + diff;
    struct route_node tmp[diff];
    memcpy(tmp, dest->nodes + dest->offset, diff);

    uint8_t i, n, metrics[total][20], *ptrs[total], refmetric[20];
    for(i = 0; i < 20; i++) refmetric[i] = srcid[i] ^ dstid[i];
    for(n = 0; n < total; n++) for(i = 0; i < 20; i++)
    {
        const struct route_node *node = n < count ? &source[n] : &tmp[n - count];
        metrics[n][i] = node->id[i] ^ dstid[i];
        ptrs[n] = metrics[n];
    }

    dest->offset = dest->count = 0;
    qsort(ptrs, total, sizeof(void*), compare);
    for(n = 0; n < total; n++)
    {
        i = (ptrs[n] - metrics[0]) / 20;
        if((i < count) && (memcmp(ptrs[n], refmetric, 20) >= 0)) continue;
        const struct route_node *node = i < count ? &source[i] : &tmp[i - count];
        if((n > 0) && (node->addr == dest->nodes[n - 1].addr) && (node->port == dest->nodes[n - 1].port)) continue;
        memcpy(dest->nodes[n].id, node->id, 20);
        dest->nodes[n].addr = node->addr;
        dest->nodes[n].port = node->port;
        if(++dest->count == 20) break;
    }
}
