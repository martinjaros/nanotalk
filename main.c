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
#include <string.h>
#include <getopt.h>
#include <arpa/inet.h>

#include "debug.h"
#include "util.h"
#include "service.h"

static void input_handler(struct service *sv)
{
    char buffer[128];
    if(fgets(buffer, sizeof(buffer), stdin) == NULL) return;
    if((strlen(buffer) > 44) && (memcmp(buffer, "ADD ", 4) == 0))
    {
        uint8_t uid[20]; char str_addr[16]; struct in_addr addr; uint16_t port;
        if(str2hex(buffer + 4, uid, 20) && (sscanf(buffer + 44, " %15[^:]:%hu", str_addr, &port) == 2) && (inet_aton(str_addr, &addr)))
            service_add(sv, uid, addr.s_addr, htons(port));
        else WARN("Parse error");
    }
    else if((strlen(buffer) == 46) && (memcmp(buffer, "DIAL ", 5) == 0))
    {
        uint8_t uid[20];
        if(str2hex(buffer + 5, uid, 20)) service_dial(sv, uid); else WARN("Hex parse error");
    }
    else if(strcmp(buffer, "ANSWER\n") == 0) service_answer(sv);
    else if(strcmp(buffer, "HANGUP\n") == 0) service_hangup(sv);
    else INFO("Unknown input");
}

static void service_handler(const uint8_t uid[20])
{
    if(uid)
    {
        char buffer[46] = "RING ";
        hex2str(uid, buffer + 5, 20);
        puts(buffer);
    }
    else puts("HANGUP");
}

int main(int argc, char **argv)
{
    const char* help = "Usage: dvsd [OPTIONS]\n"
        "   --help              Print this message\n"
        "   --version           Print the version number\n"
        "   --port=NUM          Set port number, default 5004\n"
        "   --key=FILE          Set key file, default `/etc/ssl/private/dvs.key`\n"
        "   --debug=NUM         Set trace verbosity (0 - none, 1 - error, 2 - warn, 3 - info, 4 - debug), default 3\n";

    const char* version =
        "Distributed VoIP service daemon " PACKAGE_VERSION "\n"
        "Copyright (C) 2015 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>";

    const struct option options[] =
    {
        { "help",      no_argument,       NULL, 0 },
        { "version",   no_argument,       NULL, 0 },
        { "port",      required_argument, NULL, 0 },
        { "key",       required_argument, NULL, 0 },
        { "debug",     required_argument, NULL, 0 },
        { NULL,        0,                 NULL, 0 }
    };

    char *key = "/etc/ssl/private/dvs.key";
    int index = 0, debug = 3, port = 5004;
    while(getopt_long(argc, argv, "", options, &index) == 0)
    {
        switch (index)
        {
            case 0: puts(help); return 0;
            case 1: puts(version); return 0;
            case 2: port = atol(optarg); break;
            case 3: key = optarg; break;
            case 4: debug = atol(optarg); break;
        }
    }

    debug_setlevel(debug);
    INFO("Initializing: port = %d, key = %s", port, key);

    struct event ev; event_init(&ev);
    char container[service_sizeof()]; struct service *sv = (struct service*)container;
    service_init(sv, &ev, port, key, service_handler);
    event_add(&ev, 0, (void(*)(void*))input_handler, sv);
    while(1) event_wait(&ev, -1);
}
