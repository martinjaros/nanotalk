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

#ifndef MEDIA_H
#define MEDIA_H

struct media;

size_t media_sizeof();
void media_init(struct media *media);

void media_start(struct media *media);
void media_stop(struct media *media);

void media_push(struct media *media, uint8_t *buffer, size_t len);
size_t media_pull(struct media *media, uint8_t *buffer, size_t len);

#endif /* MEDIA_H */
