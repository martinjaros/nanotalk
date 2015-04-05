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

struct event;

size_t event_sizeof();
void event_init(struct event *ev);
void event_set(struct event *ev, int fd, void (*handler)(void *args), void *args);
void event_wait(struct event *ev, int timeout);

#endif /* EVENT_H */
