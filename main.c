/*
 * Copyright (C) 2014 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>
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
#include "event.h"

static void input_handler(void *args)
{
    char buffer[128], *str = fgets(buffer, sizeof(buffer), (FILE*)args);
    if(!str) exit(EXIT_SUCCESS);
    INFO("%.*s", strlen(str) - 1, str);
}

int main(int argc, char **argv)
{
    const char* help = "Usage: dvsd [OPTIONS]\n"
        "   --help          Print this message\n"
        "   --version       Print the version number\n"
        "   --debug=NUM     Set trace verbosity (0 - none, 1 - error, 2 - warn, 3 - info, 4 - debug), default 3\n";

    const char* version =
        "Distributed VoIP service daemon " PACKAGE_VERSION "\n"
        "Copyright (C) 2015 - Martin Jaros <xjaros32@stud.feec.vutbr.cz>";

    const struct option options[] =
    {
        { "help",     no_argument,       0, 0 },
        { "version",  no_argument,       0, 0 },
        { "debug",    required_argument, 0, 0 },
        { NULL,       0,                 0, 0 }
    };

    int index = 0, debug = 3;
    while(getopt_long(argc, argv, "", options, &index) == 0)
    {
        switch (index)
        {
            case 0: puts(help); return EXIT_SUCCESS;
            case 1: puts(version); return EXIT_SUCCESS;
            case 4: debug = atol(optarg); break;
        }
    }
    debug_setlevel(debug);

    struct event ev;
    event_init(&ev);
    event_add(&ev, 0, input_handler, stdin);
    event_wait(&ev);
    return EXIT_FAILURE;
}

