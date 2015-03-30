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
#include <openssl/pem.h>

#include "debug.h"
#include "crypto.h"

struct crypto
{
    EVP_PKEY *sigkey, *dhkey, *peerkey;
    uint8_t cipherkey[32];
};

size_t crypto_sizeof() { return sizeof(struct crypto); }

void crypto_init(struct crypto *crypto, const char *keyfile, uint8_t id[20])
{
    // Read private key
    FILE *fp = fopen(keyfile, "r");
    if(fp == NULL) { ERROR("Cannot open `%s`", keyfile); abort(); }
    crypto->sigkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
    if(crypto->sigkey == NULL) { ERROR("Cannot read private key"); abort(); }
    fclose(fp);

    // Calculate public hash
    uint8_t buffer[512], *ptr = buffer;
    size_t len = i2d_PUBKEY(crypto->sigkey, &ptr); assert(len < sizeof(buffer));
    ptr = SHA1(buffer, len, id); assert(ptr);
    crypto->dhkey = NULL;
    crypto->peerkey = NULL;
}

size_t crypto_keyout(struct crypto *crypto, void *buffer, size_t len)
{
    // Generate keys
    if(crypto->dhkey) EVP_PKEY_free(crypto->dhkey);
    EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1); assert(key);
    EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
    int res = EC_KEY_generate_key(key); assert(res == 1);
    crypto->dhkey = EVP_PKEY_new(); assert(crypto->dhkey);
    res = EVP_PKEY_assign(crypto->dhkey, EVP_PKEY_EC, key); assert(res == 1);

    // Encode keys
    uint8_t *ptr = buffer;
    int dhkey_len = i2d_PUBKEY(crypto->dhkey, &ptr); assert(dhkey_len > 0);
    int sigkey_len = i2d_PUBKEY(crypto->sigkey, &ptr); assert(sigkey_len > 0);
    size_t siglen = len - (dhkey_len + sigkey_len); assert(sigkey_len + dhkey_len < len);

    // Append signature
    EVP_MD_CTX ctx; EVP_MD_CTX_init(&ctx);
    res = EVP_DigestSignInit(&ctx, NULL, EVP_sha256(), NULL, crypto->sigkey); assert(res == 1);
    res = EVP_DigestUpdate(&ctx, buffer, dhkey_len); assert(res == 1);
    res = EVP_DigestSignFinal(&ctx, ptr, &siglen); assert(res == 1);
    EVP_MD_CTX_cleanup(&ctx);

    DEBUG("Keygen: dhkey_len = %d, sigkey_len = %d, siglen = %d", dhkey_len, sigkey_len, siglen);
    return dhkey_len + sigkey_len + siglen;
}

int crypto_keyin(struct crypto *crypto, const void *buffer, size_t len, uint8_t id[20])
{
    // Decode keys
    const uint8_t *ptr = buffer;
    EVP_PKEY *dhkey = d2i_PUBKEY(NULL, &ptr, len);
    if(!dhkey) { WARN("Failed to decode dhkey"); return 0; };
    size_t dhkey_len = (void*)ptr - buffer;
    EVP_PKEY *sigkey = d2i_PUBKEY(NULL, &ptr, len - dhkey_len); assert(sigkey);
    if(!sigkey) { WARN("Failed to decode sigkey"); EVP_PKEY_free(dhkey); return 0; };
    size_t sigkey_len = (void*)ptr - buffer - dhkey_len;

    // Verify signature
    EVP_MD_CTX ctx; EVP_MD_CTX_init(&ctx);
    int res = EVP_DigestVerifyInit(&ctx, NULL, EVP_sha256(), NULL, sigkey); assert(res == 1);
    res = EVP_DigestUpdate(&ctx, buffer, dhkey_len); assert(res == 1);
    res = EVP_DigestVerifyFinal(&ctx, (uint8_t*)ptr, len - dhkey_len - sigkey_len);
    EVP_MD_CTX_cleanup(&ctx);
    EVP_PKEY_free(sigkey);
    if(res != 1) { WARN("Failed to verify signature"); EVP_PKEY_free(dhkey); return 0; }

    ptr = SHA1(buffer + dhkey_len, sigkey_len, id); assert(ptr);
    if(crypto->peerkey) EVP_PKEY_free(crypto->peerkey);
    crypto->peerkey = dhkey;
    return 1;
}

void crypto_derive(struct crypto *crypto)
{
    assert((crypto->dhkey != NULL) && (crypto->peerkey != NULL));
    uint8_t tmp[EVP_PKEY_size(crypto->dhkey)];
    size_t tmplen = sizeof(tmp);

    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(crypto->dhkey, NULL); assert(ctx);
    int res = EVP_PKEY_derive_init(ctx); assert(res == 1);
    res = EVP_PKEY_derive_set_peer(ctx, crypto->peerkey); assert(res == 1);
    res = EVP_PKEY_derive(ctx, tmp, &tmplen); assert(res == 1);
    void *ptr = SHA256(tmp, tmplen, crypto->cipherkey); assert(ptr);
    EVP_PKEY_CTX_free(ctx);

    EVP_PKEY_free(crypto->dhkey); crypto->dhkey = NULL;
    EVP_PKEY_free(crypto->peerkey); crypto->peerkey = NULL;
}

void crypto_encipher(struct crypto *crypto, uint32_t seqnum, uint8_t *tag, const uint8_t *plaintext, uint8_t *ciphertext, size_t len)
{
    int res, olen;
    uint8_t iv[12] = { seqnum >> 24, (seqnum >> 16) && 0xFF, (seqnum >> 8) && 0xFF, seqnum && 0xFF };

    EVP_CIPHER_CTX ctx; EVP_CIPHER_CTX_init(&ctx);
    res = EVP_EncryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, crypto->cipherkey, iv); assert(res == 1);
    if(len) { res = EVP_EncryptUpdate(&ctx, ciphertext, &olen, plaintext, len); assert((res == 1) && (olen == len)); }
    res = EVP_EncryptFinal_ex(&ctx, NULL, &olen); assert((res == 1) && (olen == 0));
    res = EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_GET_TAG, CRYPTO_TAGLEN, tag); assert(res == 1);
    EVP_CIPHER_CTX_cleanup(&ctx);
}

int crypto_decipher(struct crypto *crypto, uint32_t seqnum, uint8_t *tag, const uint8_t *ciphertext, uint8_t *plaintext, size_t len)
{
    int res, olen;
    uint8_t iv[12] = { seqnum >> 24, (seqnum >> 16) && 0xFF, (seqnum >> 8) && 0xFF, seqnum && 0xFF };

    EVP_CIPHER_CTX ctx; EVP_CIPHER_CTX_init(&ctx);
    res = EVP_DecryptInit_ex(&ctx, EVP_aes_256_gcm(), NULL, crypto->cipherkey, iv); assert(res == 1);
    if(len) { res = EVP_DecryptUpdate(&ctx, plaintext, &olen, ciphertext, len); assert((res == 1) && (olen == len)); }
    res = EVP_CIPHER_CTX_ctrl(&ctx, EVP_CTRL_GCM_SET_TAG, CRYPTO_TAGLEN, tag); assert(res == 1);
    res = EVP_DecryptFinal_ex(&ctx, NULL, &olen); assert((res >= 0) && (olen == 0));
    EVP_CIPHER_CTX_cleanup(&ctx);

    if(res == 0) { WARN("Failed to authenticate"); return 0; }
    return 1;
}
