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

#include "debug.h"
#include "util.h"
#include "service.h"

static void input_handler(struct service *sv)
{
    char buffer[128];
    if(fgets(buffer, sizeof(buffer), stdin) == NULL) exit(1);
    if((strlen(buffer) == 46) && (memcmp(buffer, "DIAL ", 5) == 0))
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
        "   --help          Print this message\n"
        "   --version       Print the version number\n"
        "   --port=NUM      Set port number, default 5004\n"
        "   --keyfile=FILE  Set key file path, default `key.pem`\n"
        "   --debug=NUM     Set trace verbosity (0 - none, 1 - error, 2 - warn, 3 - info, 4 - debug), default 3\n";

    const char* version =
        "Distributed VoIP service daemon " PACKAGE_VERSION "\n"
        "Copyright (C) 2015 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>";

    const struct option options[] =
    {
        { "help",     no_argument,       0, 0 },
        { "version",  no_argument,       0, 0 },
        { "port",     required_argument, 0, 0 },
        { "keyfile",  required_argument, 0, 0 },
        { "debug",    required_argument, 0, 0 },
        { NULL,       0,                 0, 0 }
    };

    char *keyfile = "key.pem";
    int index = 0, debug = 3, port = 5004;
    while(getopt_long(argc, argv, "", options, &index) == 0)
    {
        switch (index)
        {
            case 0: puts(help); return 0;
            case 1: puts(version); return 0;
            case 2: port = atol(optarg); break;
            case 3: keyfile = optarg; break;
            case 4: debug = atol(optarg); break;
        }
    }

    debug_setlevel(debug);
    INFO("Initializing: port = %d, keyfile = %s", port, keyfile);

    struct event ev; event_init(&ev);
    char container[service_sizeof()]; struct service *sv = (struct service*)container;
    service_init(sv, &ev, port, keyfile, service_handler);
    event_add(&ev, 0, (void(*)(void*))input_handler, sv);
    while(1) event_wait(&ev, -1);
}
