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

#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <stdint.h>

void* memxor(const void *__restrict s1, const void *__restrict s2, void *dest, size_t n);
int str2hex(const char *str, uint8_t *hex, size_t n);
char* hex2str(const uint8_t *hex, char *str, size_t n);

#endif /* UTIL_H */
