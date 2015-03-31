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

#ifndef ROUTE_H
#define ROUTE_H

#include <stdint.h>

struct route
{
    struct route_node
    {
        uint8_t id[20];
        uint32_t addr;
        uint16_t port;
    }
    __attribute__((packed)) nodes[20];
    uint8_t count, offset;
};

void route_init(struct route table[157]);
void route_add(struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], uint32_t addr, uint16_t port);
void route_lookup(const struct route table[157], const uint8_t srcid[20], const uint8_t dstid[20], struct route *result);
void route_merge(const struct route_node *source, uint8_t count, const uint8_t srcid[20], const uint8_t dstid[20], struct route *dest);

#endif /* ROUTE_H */
