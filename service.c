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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <arpa/inet.h>

#include "debug.h"
#include "service.h"
#include "util.h"
#include "route.h"
#include "crypto.h"
#include "media.h"

enum { MESSAGE_ROUTE_REQ, MESSAGE_ROUTE_RES, MESSAGE_KEY_REQ, MESSAGE_KEY_RES, MESSAGE_PAYLOAD, MESSAGE_HANGUP };

struct msg_route
{
    uint8_t id, src[20], dst[20];
    struct route_node nodes[0];
}
__attribute__((packed));

struct msg_payload
{
    uint8_t id;
    uint32_t seqnum;
    uint8_t tag[CRYPTO_TAGLEN], data[0];
}
__attribute__((packed));

struct service
{
    enum { STATE_IDLE, STATE_LOOKUP, STATE_HANDSHAKE, STATE_RINGING, STATE_ESTABLISHED } state;
    int epfd, sockfd, timerfd;

    uint8_t srcid[20], dstid[20];
    struct route table[157], lookup;
    uint32_t addr;
    uint16_t port;

    struct crypto *crypto;
    uint8_t keybuf[2048];
    size_t keylen;

    struct media *media;
    uint32_t seqnum_capture, seqnum_playback, packet_size;

    #define NFDS 32
    struct { void (*handler)(void *args); void *args; } fd_handlers[NFDS];
    void (*state_handler)(enum service_event event, const uint8_t uid[20], void *args);
    void *state_args;
};

size_t service_sizeof() { return sizeof(struct service) + crypto_sizeof() + media_sizeof(); }

static void lookup_handler(struct service *sv)
{
    if(sv->lookup.offset == sv->lookup.count)
    {
        WARN("No route");
        sv->state = STATE_IDLE;
        sv->state_handler(SERVICE_HANGUP, NULL, sv->state_args);
        struct itimerspec its = { { 0, 0 }, { 0, 0 } };
        int res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
        return;
    }
    if(memcmp(sv->lookup.nodes[sv->lookup.offset].id, sv->dstid, 20) == 0)
    {
        sv->addr = sv->lookup.nodes[sv->lookup.offset].addr;
        sv->port = sv->lookup.nodes[sv->lookup.offset].port;
        sv->state = STATE_HANDSHAKE;
        sv->keybuf[0] = MESSAGE_KEY_REQ;
        sv->keylen = crypto_keyout(sv->crypto, sv->keybuf + 1, sizeof(sv->keybuf) - 1) + 1;

        INFO("Lookup done; sending keys, size = %d", sv->keylen);
        struct sockaddr_in addr = { AF_INET, sv->port, { sv->addr } };
        int res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
        struct itimerspec its = { { 3, 0 }, { 3, 0 } };
        res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
        return;
    }

    struct msg_route msg = { MESSAGE_ROUTE_REQ };
    memcpy(msg.src, sv->srcid, 20);
    memcpy(msg.dst, sv->dstid, 20);
    int i; for(i = 0; i < 3; i++)
    {
        struct sockaddr_in addr = { AF_INET, sv->lookup.nodes[sv->lookup.offset].port, { sv->lookup.nodes[sv->lookup.offset].addr } };
        INFO("Sending route request to %s:%hu", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
        int res = sendto(sv->sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
        if(++sv->lookup.offset == sv->lookup.count) break;
    }
}

static void socket_handler(struct service *sv)
{
    size_t len = 0;
    int res = ioctl(sv->sockfd, FIONREAD, &len); assert(res == 0);
    uint8_t buffer[len];

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    res = recvfrom(sv->sockfd, buffer, len, 0, (struct sockaddr*)&addr, &addrlen); assert((res == len) && (addrlen == sizeof(addr)));
    if(len == 0) { WARN("Empty datagram"); return; }

    switch(buffer[0])
    {
        case MESSAGE_ROUTE_REQ:
        {
            if(len < sizeof(struct msg_route)) { WARN("Route request too short"); return; }
            struct msg_route *request = (struct msg_route*)buffer;
            route_add(sv->table, sv->srcid, request->src, addr.sin_addr.s_addr, addr.sin_port);
            char tmp[sizeof(struct msg_route) + sizeof(struct route)];
            struct route *lookup = (struct route*)(tmp + sizeof(struct msg_route));
            route_lookup(sv->table, sv->srcid, request->dst, lookup);
            INFO("Route request from %s:%hu, have %d nodes", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), lookup->count);

            struct msg_route *response = (struct msg_route*)tmp;
            response->id = MESSAGE_ROUTE_RES;
            memcpy(response->src, sv->srcid, 20);
            memcpy(response->dst, request->dst, 20);
            len = lookup->count * sizeof(struct route_node) + sizeof(struct msg_route);
            res = sendto(sv->sockfd, response, len, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
            break;
        }

        case MESSAGE_ROUTE_RES:
        {
            if(sv->state != STATE_LOOKUP) { WARN("Bad state for route response (%d)", sv->state); return; }
            if(len < sizeof(struct msg_route)) { WARN("Route response too short"); return; }
            int i; for(i = 0; i < sv->lookup.offset; i++)
                if((addr.sin_addr.s_addr == sv->lookup.nodes[i].addr) && (addr.sin_port == sv->lookup.nodes[i].port)) { i = 0; break; }
            if(i) { WARN("Address mismatch"); return; }

            struct msg_route *msg = (struct msg_route*)buffer;
            if(memcmp(msg->dst, sv->dstid, 20) != 0) { WARN("Route ID mismatch"); return; }
            route_add(sv->table, sv->srcid, msg->src, addr.sin_addr.s_addr, addr.sin_port);
            uint8_t count = (len - sizeof(struct msg_route)) / sizeof(struct route_node);
            if(count) route_merge(msg->nodes, count < 20 ? count : 20, sv->dstid, &sv->lookup);

            struct itimerspec its = { { 1, 0 }, { 1, 0 } };
            int res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
            lookup_handler(sv);
            break;
        }

        case MESSAGE_KEY_REQ:
        {
            if(sv->state == STATE_RINGING) return;
            if(sv->state == STATE_IDLE)
            {
                if(!crypto_keyin(sv->crypto, buffer + 1, len - 1, sv->dstid)) { WARN("Verification failed"); return; }
                INFO("Ringing from %s:%hu", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                route_add(sv->table, sv->srcid, sv->dstid, addr.sin_addr.s_addr, addr.sin_port);
                sv->addr = addr.sin_addr.s_addr;
                sv->port = addr.sin_port;
                sv->state = STATE_RINGING;
                sv->state_handler(SERVICE_RING, sv->dstid, sv->state_args);
            }
            else if(sv->state == STATE_ESTABLISHED)
            {
                if((addr.sin_addr.s_addr != sv->addr) || (addr.sin_port != sv->port)) { WARN("Address mismatch"); return; }
                INFO("Sending keys (response), size = %d", sv->keylen);
                res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
            }
            else WARN("Bad state for key request (%d)", sv->state); // STATE_LOOKUP, STATE_HANDSHAKE
            break;
        }

        case MESSAGE_KEY_RES:
        {
            uint8_t tmp[20];
            if(sv->state != STATE_HANDSHAKE) { WARN("Bad state for key response (%d)", sv->state); return; }
            if((addr.sin_addr.s_addr != sv->addr) || (addr.sin_port != sv->port)) { WARN("Address mismatch"); return; }
            if(!crypto_keyin(sv->crypto, buffer + 1, len - 1, tmp)) { WARN("Verification failed"); return; }
            if(memcmp(tmp, sv->dstid, 20) != 0) { WARN("Peer ID mismatch"); return; }
            crypto_derive(sv->crypto);
            sv->state = STATE_ESTABLISHED;

            INFO("Connected to %s:%hu", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            media_start(sv->media);
            sv->seqnum_capture = sv->seqnum_playback = 0;
            struct itimerspec its = { { 0, 20000000 }, { 0, 20000000 } };
            res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
            break;
        }

        case MESSAGE_PAYLOAD:
        {
            if(sv->state != STATE_ESTABLISHED) { DEBUG("Bad state for payload (%d)", sv->state); return; }
            if((addr.sin_addr.s_addr != sv->addr) || (addr.sin_port != sv->port)) { WARN("Address mismatch"); return; }
            if(len < sizeof(struct msg_payload)) { WARN("Payload too short"); return; }
            struct msg_payload *msg = (struct msg_payload*)buffer;
            uint32_t seqnum = ntohl(msg->seqnum);
            if(seqnum < sv->seqnum_playback) { WARN("Out of sequence %d < %d", seqnum, sv->seqnum_playback); return; }

            uint8_t tmp[len - sizeof(struct msg_payload)];
            if(crypto_decipher(sv->crypto, seqnum, msg->tag, msg->data, tmp, sizeof(tmp)) == 0) { WARN("Corrupted message"); return; }
            if(seqnum > sv->seqnum_playback)
            {
                WARN("Packet lost");
                media_push(sv->media, NULL, 0);
                sv->seqnum_playback = seqnum;
            }

            sv->seqnum_playback++;
            media_push(sv->media, tmp, sizeof(tmp));
            break;
        }

        case MESSAGE_HANGUP:
        {
            if(sv->state < STATE_HANDSHAKE) { WARN("Bad state for hangup (%d)", sv->state); return; } // STATE_IDLE, STATE_LOOKUP
            if((addr.sin_addr.s_addr != sv->addr) || (addr.sin_port != sv->port)) { WARN("Address mismatch"); return; }
            struct itimerspec its = { { 0, 0 }, { 0, 0 } };
            res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
            if(sv->state == STATE_ESTABLISHED) media_stop(sv->media);
            INFO("Disconnected by peer");
            sv->state = STATE_IDLE;
            sv->state_handler(SERVICE_HANGUP, NULL, sv->state_args);
            break;
        }

        default:
            WARN("Unknown message id (%d) from %s:%hu, len = %d", buffer[0], inet_ntoa(addr.sin_addr), ntohs(addr.sin_port), len);
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
            lookup_handler(sv);
            break;

        case STATE_HANDSHAKE:
            DEBUG("Sending keys (timeout), size = %d", sv->keylen);
            struct sockaddr_in addr = { AF_INET, sv->port, { sv->addr } };
            res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
            break;

        case STATE_ESTABLISHED:
        {
            uint8_t buffer[sv->packet_size];
            size_t len = media_pull(sv->media, buffer, sizeof(buffer));

            uint8_t tmp[sizeof(struct msg_payload) + len]; struct msg_payload *msg = (struct msg_payload*)tmp;
            msg->id = MESSAGE_PAYLOAD;
            msg->seqnum = htonl(sv->seqnum_capture);
            crypto_encipher(sv->crypto, sv->seqnum_capture++, msg->tag, buffer, msg->data, len);

            DEBUG("Sending payload, size = %d", sizeof(tmp));
            struct sockaddr_in addr = { AF_INET, sv->port, { sv->addr } };
            res = sendto(sv->sockfd, tmp, sizeof(tmp), 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
            break;
        }

        default: // STATE_IDLE, STATE_RINGING
            WARN("No work for timer");
    }
}

void service_add(struct service *sv, const uint8_t uid[20], uint32_t addr, uint16_t port)
{
    route_add(sv->table, sv->srcid, uid, addr, port);
}

void service_dial(struct service *sv, const uint8_t uid[20])
{
    if(sv->state != STATE_IDLE) { WARN("Bad state to dial (%d)", sv->state); return; }
    sv->state = STATE_LOOKUP;
    memcpy(sv->dstid, uid, 20);
    route_lookup(sv->table, sv->srcid, sv->dstid, &sv->lookup);
    struct itimerspec its = { { 1, 0 }, { 1, 0 } };
    int res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
    lookup_handler(sv);
}

void service_answer(struct service *sv)
{
    if(sv->state != STATE_RINGING) { WARN("Bad state to answer (%d)", sv->state); return; }
    sv->state = STATE_ESTABLISHED;
    sv->keybuf[0] = MESSAGE_KEY_RES;
    sv->keylen = crypto_keyout(sv->crypto, sv->keybuf + 1, sizeof(sv->keybuf) - 1) + 1;

    INFO("Sending keys, size = %d", sv->keylen);
    struct sockaddr_in addr = { AF_INET, sv->port, { sv->addr } };
    int res = sendto(sv->sockfd, sv->keybuf, sv->keylen, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
    crypto_derive(sv->crypto);

    media_start(sv->media);
    sv->seqnum_capture = sv->seqnum_playback = 0;
    struct itimerspec its = { { 0, 20000000 }, { 0, 20000000 } };
    res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
}

void service_hangup(struct service *sv)
{
    if(sv->state == STATE_IDLE) { DEBUG("Hangup while idle"); return; }
    struct itimerspec its = { { 0, 0 }, { 0, 0 } };
    int res = timerfd_settime(sv->timerfd, 0, &its, NULL); assert(res == 0);
    if(sv->state > STATE_LOOKUP) // STATE_HANDSHAKE, STATE_RINGING, STATE_ESTABLISHED
    {
        uint8_t msg = MESSAGE_HANGUP;
        struct sockaddr_in addr = { AF_INET, sv->port, { sv->addr } };
        res = sendto(sv->sockfd, &msg, 1, 0, (struct sockaddr*)&addr, sizeof(addr)); assert(res > 0);
    }
    sv->state = STATE_IDLE; INFO("Disconnected");
}

void service_pollfd(struct service *sv, int fd, void (*handler)(void *args), void *args)
{
    struct epoll_event event = { .events = EPOLLIN, .data = { .fd = fd } }; assert(fd < NFDS);
    int res = epoll_ctl(sv->epfd, handler ? EPOLL_CTL_ADD : EPOLL_CTL_DEL, fd, &event); assert(res == 0);
    sv->fd_handlers[fd].handler = handler;
    sv->fd_handlers[fd].args = args;
}

void service_wait(struct service *sv, int timeout)
{
    struct epoll_event event;
    if(epoll_wait(sv->epfd, &event, 1, timeout) == 1)
        sv->fd_handlers[event.data.fd].handler(sv->fd_handlers[event.data.fd].args);
}

void service_init(struct service *sv, uint16_t port, const char *key, const char *capture, const char *playback, uint32_t bitrate,
        void (*handler)(enum service_event event, const uint8_t uid[20], void *args), void *args)
{
    sv->state = STATE_IDLE;
    sv->state_handler = handler;
    sv->state_args = args;
    sv->epfd = epoll_create1(0); assert(sv->epfd > 0);
    sv->timerfd = timerfd_create(CLOCK_MONOTONIC, 0); assert(sv->timerfd > 0);
    service_pollfd(sv, sv->timerfd, (void(*)(void*))timer_handler, sv);

    const int optval = 1;
    const struct sockaddr_in addr = { AF_INET, htons(port) };
    sv->sockfd = socket(AF_INET, SOCK_DGRAM, 0); assert(sv->sockfd > 0);
    int res = setsockopt(sv->sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)); assert(res == 0);
    if(bind(sv->sockfd, (struct sockaddr*)&addr, sizeof(addr)) != 0) { ERROR("Cannot bind socket"); abort(); }
    service_pollfd(sv, sv->sockfd, (void(*)(void*))socket_handler, sv);

    route_init(sv->table);
    sv->crypto = (struct crypto*)((void*)sv + sizeof(struct service));
    crypto_init(sv->crypto, key, sv->srcid);
    sv->media = (struct media*)((void*)sv + sizeof(struct service) + crypto_sizeof());
    media_init(sv->media, capture, playback);
    sv->packet_size = bitrate / 8 / 50;
    INFO("Media: bitrate = %u, capture = %s, playback = %s", bitrate, capture, playback);

    char tmp[128]; INFO("User ID is %.40s", hexify(sv->srcid, tmp, 20));
}
