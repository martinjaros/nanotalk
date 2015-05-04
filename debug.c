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
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <syscall.h>

#include "debug.h"

static unsigned int threshold = 0;

void debug_setlevel(unsigned int level)
{
    threshold = level;
}

void debug_printf(unsigned int level, const char *file, unsigned int line, const char *msg, ...)
{
    if(level > threshold) return;

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    char buffer[1024];
    int num = snprintf(buffer, sizeof(buffer), "%.2lu:%.2lu:%.2lu.%.3lu [%lu] (%s:%u) %s - ",
                       ts.tv_sec / 3600 % 24, ts.tv_sec / 60 % 60, ts.tv_sec % 60, ts.tv_nsec / 1000000,
                       syscall(SYS_gettid), file, line, level == 1 ? "ERROR" : level == 2 ? "WARN" : level == 3 ? "INFO" : "DEBUG");
    assert(num > 0);

    va_list args;
    va_start(args, msg);
    int vnum = vsnprintf(buffer + num, sizeof(buffer) - num - 1, msg, args); assert(vnum > 0);
    buffer[num + vnum] = '\n';
    va_end(args);

    num = write(STDERR_FILENO, buffer, num + vnum + 1); assert(num > 0);
}
