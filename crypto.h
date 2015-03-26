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

#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>

struct crypto;

size_t crypto_sizeof();
void crypto_init(struct crypto *crypto, const char *keyring, uint8_t id[20]);

size_t crypto_keyout(struct crypto *crypto, void *buffer, size_t len);
int crypto_keyin(struct crypto *crypto, const void *buffer, size_t len, uint8_t id[20]);
void crypto_derive(struct crypto *crypto);

#define CRYPTO_TAGLEN 8
void crypto_encipher(struct crypto *crypto, uint32_t seqnum, uint8_t *tag, const uint8_t *plaintext, uint8_t *ciphertext, size_t len);
int crypto_decipher(struct crypto *crypto, uint32_t seqnum, uint8_t *tag, const uint8_t *ciphertext, uint8_t *plaintext, size_t len);

#endif /* CRYPTO_H */
