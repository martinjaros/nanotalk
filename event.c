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
#include <sys/epoll.h>

#include "event.h"

#define NFDS 32

struct event
{
    int epfd;
    struct { void (*handler)(void *args); void *args; } handlers[NFDS];
};

size_t event_sizeof() { return sizeof(struct event); }

void event_init(struct event *ev)
{
    ev->epfd = epoll_create1(0); assert(ev->epfd >= 0);
}

void event_set(struct event *ev, int fd, void (*handler)(void *args), void *args)
{
    assert(fd < NFDS);
    struct epoll_event event = { .events = EPOLLIN, .data = { .fd = fd } };
    int res = epoll_ctl(ev->epfd, handler ? EPOLL_CTL_ADD : EPOLL_CTL_DEL, fd, &event); assert(res == 0);
    ev->handlers[fd].handler = handler;
    ev->handlers[fd].args = args;
}

void event_wait(struct event *ev, int timeout)
{
    struct epoll_event event;
    if(epoll_wait(ev->epfd, &event, 1, timeout) == 1)
    {
        assert(ev->handlers[event.data.fd].handler);
        ev->handlers[event.data.fd].handler(ev->handlers[event.data.fd].args);
    }
}
