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

#ifndef EVENT_H
#define EVENT_H

#define EVENT_NFDS 32

#include <assert.h>
#include <sys/epoll.h>

struct event
{
    int epfd;
    struct { void (*handler)(void *args); void *args; } handlers[EVENT_NFDS];
};

static inline void event_init(struct event *ev)
{
    ev->epfd = epoll_create1(0); assert(ev->epfd >= 0);
}

static inline void event_add(struct event *ev, int fd, void (*handler)(void *args), void *args)
{
    assert(fd < EVENT_NFDS);
    struct epoll_event event = { .events = EPOLLIN, .data = { .fd = fd } };
    int res = epoll_ctl(ev->epfd, handler ? EPOLL_CTL_ADD : EPOLL_CTL_DEL, fd, &event); assert(res == 0);
    ev->handlers[fd].handler = handler;
    ev->handlers[fd].args = args;
}

static inline void event_wait(struct event *ev)
{
    while(1)
    {
        struct epoll_event event;
        if(epoll_wait(ev->epfd, &event, 1, -1) == 1)
        {
            assert(ev->handlers[event.data.fd].handler);
            ev->handlers[event.data.fd].handler(ev->handlers[event.data.fd].args);
        }
    }
}

#endif /* EVENT_H */
