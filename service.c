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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>

#include "debug.h"
#include "service.h"
#include "route.h"
#include "crypto.h"
#include "media.h"

enum { MESSAGE_ROUTE_REQ, MESSAGE_ROUTE_RES, MESSAGE_KEY_REQ, MESSAGE_KEY_RES, MESSAGE_PAYLOAD };

struct msg_payload
{
    uint8_t id;
    uint32_t seqnum;
    uint8_t tag[CRYPTO_TAGLEN], data[0];
}
__attribute__((packed));

struct service
{
    enum { STATE_IDLE, STATE_RINGING, STATE_LOOKUP, STATE_HANDSHAKE, STATE_ESTABLISHED } state;
    int sockfd, timerfd;

    uint8_t srcid[20], dstid[20];
    struct route table[157], lookup;
    struct sockaddr_in addr;

    struct crypto *crypto;
    uint8_t keybuf[2048];
    size_t keylen;

    struct media *media;
    uint32_t seqnum_capture, seqnum_playback;

    void (*handler)(const uint8_t uid[20]);
};

size_t service_sizeof() { return sizeof(struct service) + crypto_sizeof() + media_sizeof(); }

static void socket_handler(struct service *sv)
{
    size_t len = 0;
    int res = ioctl(sv->sockfd, FIONREAD, &len); assert(res == 0);
    uint8_t buffer[len];

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    res = recvfrom(sv->sockfd, buffer, len, 0, (struct sockaddr*)&addr, &addrlen); assert((res == len) && (addrlen == sizeof(addr)));
    DEBUG("Received datagram from `%s:%hu, len = %d`", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), len);
    if(len == 0) return;

    switch(buffer[0])
    {
        case MESSAGE_ROUTE_REQ:
        {
            // TODO
            break;
        }

        case MESSAGE_ROUTE_RES:
        {
            // TODO
            break;
        }

        case MESSAGE_KEY_REQ:
        {
            if(sv->state == STATE_IDLE)
            {
                if(!crypto_keyin(sv->crypto, buffer + 1, len - 1, sv->dstid)) { WARN("Verification failed"); return; }
                INFO("Ringing from %s:%hu", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                memcpy(&sv->addr, &addr, sizeof(addr));
                sv->state = STATE_RINGING;
                sv->handler(sv->dstid);
            }
            else if(sv->state == STATE_ESTABLISHED)
            {
                if(memcmp(&sv->addr, &addr, sizeof(addr)) != 0) { WARN("Address mismatch"); return; }
                INFO("Sending keys (response), size = %d", sv->keylen);
                res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
            }
            else WARN("Bad state for key req (%d)", sv->state); // STATE_RINGING, STATE_LOOKUP, STATE_HANDSHAKE
            break;
        }

        case MESSAGE_KEY_RES:
        {
            uint8_t tmp[20];
            if(sv->state != STATE_HANDSHAKE) { WARN("Bad state for key res (%d)", sv->state); return; }
            if(memcmp(&sv->addr, &addr, sizeof(addr)) != 0) { WARN("Address mismatch"); return; }
            if(!crypto_keyin(sv->crypto, buffer + 1, len - 1, tmp)) { WARN("Verification failed"); return; }
            if(memcmp(tmp, sv->dstid, 20) != 0) { WARN("Peer ID mismatch"); return; }
            crypto_derive(sv->crypto);
            sv->state = STATE_ESTABLISHED;
            sv->seqnum_capture = 0;
            sv->seqnum_playback = 0;

            INFO("Connected to %s:%hu", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            struct itimerspec its = { { 0, 20000000 }, { 0, 20000000 } };
            res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
            media_start(sv->media);
            break;
        }

        case MESSAGE_PAYLOAD:
        {
            if(sv->state != STATE_ESTABLISHED) { DEBUG("Bad state for payload (%d)", sv->state); return; }
            if(memcmp(&sv->addr, &addr, sizeof(addr)) != 0) { WARN("Address mismatch"); return; }
            if(len < sizeof(struct msg_payload)) { WARN("Payload too short"); return; }
            struct msg_payload *msg = (struct msg_payload*)buffer;
            uint32_t seqnum = ntohl(msg->seqnum);

            if(seqnum < sv->seqnum_playback) { WARN("Out of sequence %d < %d", seqnum, sv->seqnum_playback); return; }
            uint8_t tmp[len - sizeof(struct msg_payload)];
            if(crypto_decipher(sv->crypto, seqnum, msg->tag, msg->data, tmp, sizeof(tmp)) == 0) { WARN("Corrupted message"); return; }
            if(sizeof(tmp) == 0)
            {
                INFO("Disconnected by peer");
                struct itimerspec its = { { 0, 0 }, { 0, 0 } };
                res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
                media_stop(sv->media);
                sv->state = STATE_IDLE;
                sv->handler(NULL);
                return;
            }
            if(seqnum > sv->seqnum_playback)
            {
                WARN("Packet lost");
                sv->seqnum_playback = seqnum;
                media_push(sv->media, NULL, 0);
            }

            media_push(sv->media, tmp, sizeof(tmp));
            sv->seqnum_playback++;
            break;
        }

        default:
            WARN("Unknown message type = %d", buffer[0]);
    }
}

static void timer_handler(struct service *sv)
{
    uint64_t n;
    int res = read(sv->timerfd, &n, 8); assert(res == 8);
    if(n > 1) WARN("Missed %d timer ticks", n - 1);

    switch(sv->state)
    {
        case STATE_LOOKUP:
        {
            // TODO
            break;
        }

        case STATE_HANDSHAKE:
        {
            INFO("Sending keys (request), size = %d", sv->keylen);
            res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&sv->addr, sizeof(struct sockaddr_in)); assert(res > 0);
            break;
        }

        case STATE_ESTABLISHED:
        {
            uint8_t buffer[80];
            size_t len = media_pull(sv->media, buffer, sizeof(buffer));

            uint8_t tmp[sizeof(struct msg_payload) + len]; struct msg_payload *msg = (struct msg_payload*)tmp;
            msg->id = MESSAGE_PAYLOAD;
            msg->seqnum = htonl(sv->seqnum_capture);
            crypto_encipher(sv->crypto, sv->seqnum_capture++, msg->tag, buffer, msg->data, len);

            DEBUG("Sending payload, size = %d", sizeof(tmp));
            res = sendto(sv->sockfd, tmp, sizeof(tmp), 0, (struct sockaddr*)&sv->addr, sizeof(struct sockaddr_in)); assert(res > 0);
            break;
        }

        default: // STATE_IDLE, STATE_RINGING
            DEBUG("No work for timer");
    }
}

void service_dial(struct service *sv, const uint8_t uid[20])
{
    if(sv->state != STATE_IDLE) { WARN("Bad state to dial (%d)", sv->state); return; }
    sv->state = STATE_LOOKUP;
    memcpy(sv->dstid, uid, 20);

    // TODO

    struct itimerspec its = { { 1, 0 }, { 1, 0 } };
    int res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
}

void service_answer(struct service *sv)
{
    if(sv->state != STATE_RINGING) { WARN("Bad state to answer (%d)", sv->state); return; }
    sv->state = STATE_ESTABLISHED;
    sv->seqnum_capture = 0;
    sv->seqnum_playback = 0;
    sv->keybuf[0] = MESSAGE_KEY_RES;
    sv->keylen = crypto_keyout(sv->crypto, sv->keybuf + 1, sizeof(sv->keybuf) - 1) + 1;

    INFO("Sending keys (response), size = %d", sv->keylen);
    int res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&sv->addr, sizeof(struct sockaddr_in)); assert(res > 0);
    crypto_derive(sv->crypto);
    media_start(sv->media);
    struct itimerspec its = { { 0, 20000000 }, { 0, 20000000 } };
    res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
}

void service_hangup(struct service *sv)
{
    if(sv->state == STATE_IDLE) { WARN("Bad state to hangup (0)"); return; }
    if(sv->state > STATE_RINGING) // STATE_LOOKUP, STATE_HANDSHAKE, STATE_ESTABLISHED
    {
        struct itimerspec its = { { 0, 0 }, { 0, 0 } };
        int res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
    }
    if(sv->state == STATE_ESTABLISHED)
    {
        INFO("Disconnecting");
        media_stop(sv->media);
        struct msg_payload msg = { MESSAGE_PAYLOAD, -1 };
        crypto_encipher(sv->crypto, -1, msg.tag, NULL, NULL, 0);
        int res = sendto(sv->sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&sv->addr, sizeof(struct sockaddr_in)); assert(res > 0);
    }
    sv->state = STATE_IDLE;
}

void service_init(struct service *sv, struct event *ev, uint16_t port, const char *keyfile, void (*handler)(const uint8_t uid[20]))
{
    sv->state = STATE_IDLE;
    sv->timerfd = timerfd_create(CLOCK_MONOTONIC, 0); assert(sv->timerfd > 0);
    event_add(ev, sv->timerfd, (void(*)(void*))timer_handler, sv);

    const int optval = 1;
    const struct sockaddr_in addr = { AF_INET, htons(port) };
    sv->sockfd = socket(AF_INET, SOCK_DGRAM, 0); assert(sv->sockfd > 0);
    int res = setsockopt(sv->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); assert(res == 0);
    if(bind(sv->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) { ERROR("Cannot bind socket"); abort(); }
    event_add(ev, sv->sockfd, (void(*)(void*))socket_handler, sv);

    route_init(sv->table);
    sv->crypto = (struct crypto*)((void*)sv + sizeof(struct service));
    crypto_init(sv->crypto, keyfile, sv->srcid);
    sv->media = (struct media*)((void*)sv + sizeof(struct service) + crypto_sizeof());
    media_init(sv->media);
    sv->handler = handler;
}
