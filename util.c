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

#include "util.h"

void* memxor(const void *__restrict s1, const void *__restrict s2, void *dest, size_t n)
{
    const uint8_t *u8s1 = s1, *u8s2 = s2; uint8_t *u8dest = dest;
    int i; for(i = 0; i < n; i++) u8dest[i] = u8s1[i] ^ u8s2[i];
    return dest;
}

int str2hex(const char *str, uint8_t *hex, size_t n)
{
    int i; for(i = 0; i < n; i++)
    {
        char ch = str[i * 2], cl = str[i * 2 + 1];
        if((cl >= '0') && (cl <= '9')) cl -= '0'; else if((cl >='A') && (cl <= 'F')) cl += 10 - 'A'; else return 0;
        if((ch >= '0') && (ch <= '9')) ch -= '0'; else if((ch >='A') && (ch <= 'F')) ch += 10 - 'A'; else return 0;
        hex[i] = (ch << 4) + cl;
    }
    return 1;
}

char* hex2str(const uint8_t *hex, char *str, size_t n)
{
    const char map[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    int i; for(i = 0; i < n; i++)
    {
        str[i * 2] = map[hex[i] >> 4];
        str[i * 2 + 1] = map[hex[i] & 0xF];
    }
    return str;
}
