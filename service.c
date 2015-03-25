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

#include <assert.h>
#include <stddef.h>

#include "debug.h"
#include "service.h"
#include "route.h"

struct service
{
    struct route table[157];

    void (*handler)(const uint8_t uid[20], void *args);
    void *args;
};

size_t service_sizeof() { return sizeof(struct service); }

void service_init(struct service *sv, struct event *ev, uint16_t port, const char *keyfile,
                  void (*handler)(const uint8_t uid[20], void *args), void *args)
{
    uint8_t srcid[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    uint8_t dstid[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8 };
    uint8_t test1[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9 };
    uint8_t test2[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10 };
    struct route route1, route2, route3;

    route_init(sv->table);
    route_add(sv->table, srcid, test1, 1, 0);
    route_add(sv->table, srcid, dstid, 0, 0);

    int i;
    route_lookup(sv->table, srcid, dstid, &route1);
    for(i = 0; i < route1.count; i++) INFO("Route1: %d", route1.nodes[i].addr);

    route_add(sv->table, srcid, test2, 2, 0);
    route_lookup(sv->table, srcid, dstid, &route2);
    for(i = 0; i < route2.count; i++) INFO("Route2: %d", route2.nodes[i].addr);

    route_merge(&route1, &route2, dstid, &route3);
    for(i = 0; i < route3.count; i++) INFO("Route3: %d", route3.nodes[i].addr);

    sv->handler = handler;
    sv->args = args;
}

void service_dial(struct service *sv, const uint8_t uid[20])
{
    if(sv->handler) sv->handler(uid, sv->args);
}

void service_answer(struct service *sv)
{
    if(sv->handler) sv->handler(NULL, sv->args);
}

void service_hangup(struct service *sv)
{
    if(sv->handler) sv->handler(NULL, sv->args);
}
