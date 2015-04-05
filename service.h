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

#ifndef SERVICE_H
#define SERVICE_H

#include <stdint.h>
#include "event.h"

struct service;

size_t service_sizeof();
void service_init(struct service *sv, struct event *ev, uint16_t port, const char *key, void (*handler)(const uint8_t uid[20]));
void service_add(struct service *sv, const uint8_t uid[20], uint32_t addr, uint16_t port);
void service_dial(struct service *sv, const uint8_t uid[20]);
void service_answer(struct service *sv);
void service_hangup(struct service *sv);

#endif /* SERVICE_H */
