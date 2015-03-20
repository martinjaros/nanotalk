/*
 * Copyright (C) 2014 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
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

#ifndef DEBUG_H
#define DEBUG_H

#define ERROR(msg, ...) debug_printf(1, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define WARN(msg, ...)  debug_printf(2, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define INFO(msg, ...)  debug_printf(3, __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define DEBUG(msg, ...) debug_printf(4, __FILE__, __LINE__, msg, ##__VA_ARGS__)

void debug_setlevel(int level);
void debug_printf(int level, const char *file, int line, const char *msg, ...);

#endif /* DEBUG_H */
