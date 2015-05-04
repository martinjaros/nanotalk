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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include "debug.h"
#include "util.h"
#include "service.h"

static void input_handler(struct service *sv)
{
    int len = 0, res = ioctl(STDIN_FILENO, FIONREAD, &len); assert(res == 0);
    char buffer[len + 1], *ptr; buffer[len] = 0;
    if(read(STDIN_FILENO, buffer, len) != len) return;

    ptr = strtok(buffer, "\n");
    while(ptr)
    {
        len = strlen(ptr); DEBUG("Got input: %s", ptr);
        if((len > 44) && (memcmp(ptr, "ADD ", 4) == 0))
        {
            uint8_t uid[20]; char c, str_addr[16]; struct in_addr addr; uint16_t port;
            if(unhexify(ptr + 4, uid, 20) && (sscanf(ptr + 44, " %15[^:]:%hu%c", str_addr, &port, &c) == 2) && (inet_aton(str_addr, &addr)))
            { INFO("Adding %s", ptr + 4); service_add(sv, uid, addr.s_addr, htons(port)); } else WARN("Parse error");
        }
        else if((len == 45) && (memcmp(ptr, "DIAL ", 5) == 0))
        {
            uint8_t uid[20];
            if(unhexify(ptr + 5, uid, 20)) { INFO("Dialing %s", ptr + 5); service_dial(sv, uid); } else WARN("Parse error");
        }
        else if((len == 6) && (memcmp(ptr, "ANSWER", 6) == 0)) { INFO("Answering"); service_answer(sv); }
        else if((len == 6) && (memcmp(ptr, "HANGUP", 6) == 0)) { INFO("Hangup"); service_hangup(sv); }
        else INFO("Unknown input: %s", ptr);
        ptr = strtok(NULL, "\n");
    }
}

static void service_handler(enum service_event event, const uint8_t uid[20], struct service *sv)
{
    switch(event)
    {
        case SERVICE_RING:
        {
            char buffer[46] = "RING "; buffer[45] = '\n';
            hexify(uid, buffer + 5, 20);
            int res = write(STDOUT_FILENO, buffer, sizeof(buffer)); assert(res > 0);
            break;
        }

        case SERVICE_HANGUP:
        {
            const char buffer[7] = "HANGUP\n";
            int res = write(STDOUT_FILENO, buffer, sizeof(buffer)); assert(res > 0);
            break;
        }
    }
}

int main(int argc, char **argv)
{
    const char* version = PACKAGE_STRING "\nCopyright (C) 2015 - Martin Jaros <" PACKAGE_BUGREPORT ">\n" PACKAGE_URL;
    const char* help = "Usage: %s [OPTIONS]\n"
        "   -h, --help       Print help\n"
        "   -v, --version    Print version\n"
        "   -p, --port       Set port number, default 5004\n"
        "   -k, --key        Set key file, default `/etc/ssl/private/nanotalk.key`\n"
        "   -r, --bitrate    Set bitrate, default 32000\n"
        "   -i, --capture    Set ALSA capture device\n"
        "   -o, --playback   Set ALSA playback device\n"
        "   -d, --debug      Set trace verbosity (0 - none, 1 - error, 2 - warn, 3 - info, 4 - debug), default 3\n";

    const struct option options[] =
    {
        { "help",     no_argument,       NULL, 'h' },
        { "version",  no_argument,       NULL, 'v' },
        { "port",     required_argument, NULL, 'p' },
        { "key",      required_argument, NULL, 'k' },
        { "bitrate",  required_argument, NULL, 'r' },
        { "capture",  required_argument, NULL, 'i' },
        { "playback", required_argument, NULL, 'o' },
        { "debug",    required_argument, NULL, 'd' },
        { 0 }
    };

    char opt, *key = "/etc/ssl/private/nanotalk.key", *capture = "default", *playback = "default";
    unsigned int debug = 3, port = 5004, bitrate = 32000;
    while((opt = getopt_long(argc, argv, "hvp:k:r:i:o:d:", options, NULL)) != -1)
    {
        switch(opt)
        {
            case 'h': printf(help, *argv); return 0;
            case 'v': puts(version); return 0;
            case 'p': port = atol(optarg); break;
            case 'k': key = optarg; break;
            case 'r': bitrate = atol(optarg); break;
            case 'i': capture = optarg; break;
            case 'o': playback = optarg; break;
            case 'd': debug = atol(optarg); break;
        }
    }

    debug_setlevel(debug);
    INFO("Initializing: port = %u, key = %s", port, key);

    char container[service_sizeof()]; struct service *sv = (struct service*)container;
    service_init(sv, port, key, capture, playback, bitrate, (void(*)(enum service_event, const uint8_t*, void*))service_handler, sv);
    service_pollfd(sv, STDIN_FILENO, (void(*)(void*))input_handler, sv);
    while(1) service_wait(sv, -1);
}
