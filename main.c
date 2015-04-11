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
#include <sys/un.h>
#include <arpa/inet.h>

#include "debug.h"
#include "util.h"
#include "service.h"

struct app
{
    int listenfd, clientfd;
    struct service *sv;
};

static void connection_handler(struct app *app);
static void input_handler(struct app *app)
{
    size_t len = 0;
    int res = ioctl(app->clientfd, FIONREAD, &len); assert(res == 0);
    char buffer[len + 1], *ptr; buffer[len] = 0;

    res = recv(app->clientfd, buffer, len, 0);
    if(res <= 0)
    {
        INFO("Client disconnected");
        close(app->clientfd); app->clientfd = -1;
        service_pollfd(app->sv, app->listenfd, (void(*)(void*))connection_handler, app);
        service_hangup(app->sv);
        return;
    }
    assert(res == len);

    ptr = strtok(buffer, "\n");
    while(ptr)
    {
        len = strlen(ptr); DEBUG("Got input: %s", ptr);
        if((len > 44) && (memcmp(ptr, "ADD ", 4) == 0))
        {
            uint8_t uid[20]; char c, str_addr[16]; struct in_addr addr; uint16_t port;
            if(unhexify(ptr + 4, uid, 20) && (sscanf(ptr + 44, " %15[^:]:%hu%c", str_addr, &port, &c) == 2) && (inet_aton(str_addr, &addr)))
            { INFO("Adding %s", ptr + 4); service_add(app->sv, uid, addr.s_addr, htons(port)); } else WARN("Parse error");
        }
        else if((len == 45) && (memcmp(ptr, "DIAL ", 5) == 0))
        {
            uint8_t uid[20];
            if(unhexify(ptr + 5, uid, 20)) { INFO("Dialing %s", ptr + 5); service_dial(app->sv, uid); } else WARN("Parse error");
        }
        else if((len == 6) && (memcmp(ptr, "ANSWER", 6) == 0)) { INFO("Answering"); service_answer(app->sv); }
        else if((len == 6) && (memcmp(ptr, "HANGUP", 6) == 0)) { INFO("Hangup"); service_hangup(app->sv); }
        else INFO("Unknown input: %s", ptr);
        ptr = strtok(NULL, "\n");
    }
}

static void connection_handler(struct app *app)
{
    app->clientfd = accept(app->listenfd, NULL, NULL); assert(app->clientfd > 0);
    service_pollfd(app->sv, app->clientfd, (void(*)(void*))input_handler, app);
    service_pollfd(app->sv, app->listenfd, NULL, NULL);
    INFO("Client connected");
}

static void service_handler(const uint8_t uid[20], struct app *app)
{
    if(uid)
    {
        if(app->clientfd == -1) { WARN("No client"); service_hangup(app->sv); return; }
        char buffer[46] = "RING "; buffer[45] = '\n';
        hexify(uid, buffer + 5, 20);
        int res = send(app->clientfd, buffer, sizeof(buffer), 0); assert(res > 0);
    }
    else
    {
        if(app->clientfd == -1) { WARN("No client"); return; }
        const char buffer[] = "HANGUP\n";
        int res = send(app->clientfd, buffer, sizeof(buffer), 0); assert(res > 0);
    }
}

int main(int argc, char **argv)
{
    const char* version = PACKAGE_STRING "\nCopyright (C) 2015 - Martin Jaros <" PACKAGE_BUGREPORT ">\n" PACKAGE_URL;
    const char* help = "Usage: %s [OPTIONS]\n"
        "   --help              Print help\n"
        "   --version           Print version\n"
        "   --port=NUM          Set port number, default 5004\n"
        "   --key=FILE          Set key file, default `/etc/ssl/private/dvs.key`\n"
        "   --socket=FILE       Set unix socket, default `/tmp/dvs`\n"
        "   --capture=DEV       Set ALSA capture device\n"
        "   --playback=DEV      Set ALSA playback device\n"
        "   --debug=NUM         Set trace verbosity (0 - none, 1 - error, 2 - warn, 3 - info, 4 - debug), default 3\n";

    const struct option options[] =
    {
        { "help",      no_argument,       NULL, 0 },
        { "version",   no_argument,       NULL, 0 },
        { "port",      required_argument, NULL, 0 },
        { "key",       required_argument, NULL, 0 },
        { "socket",    required_argument, NULL, 0 },
        { "capture",   required_argument, NULL, 0 },
        { "playback",  required_argument, NULL, 0 },
        { "debug",     required_argument, NULL, 0 },
        { NULL,        0,                 NULL, 0 }
    };

    struct sockaddr_un sockaddr = { AF_UNIX, "/tmp/dvs" };
    char *key = "/etc/ssl/private/dvs.key", *capture = "default", *playback = "default";
    int index = 0, debug = 3, port = 5004;
    while(getopt_long(argc, argv, "", options, &index) == 0)
    {
        switch (index)
        {
            case 0: printf(help, *argv); return 0;
            case 1: puts(version); return 0;
            case 2: port = atol(optarg); break;
            case 3: key = optarg; break;
            case 4: strcpy(sockaddr.sun_path, optarg); break;
            case 5: capture = optarg; break;
            case 6: playback = optarg; break;
            case 7: debug = atol(optarg); break;
        }
    }

    debug_setlevel(debug);
    INFO("Initializing: port = %d, key = %s, socket = %s", port, key, sockaddr.sun_path);

    char container[service_sizeof()];
    struct app app = { socket(AF_UNIX, SOCK_STREAM, 0), -1, (struct service*)container }; assert(app.listenfd > 0);

    unlink(sockaddr.sun_path);
    if(bind(app.listenfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) != 0) { ERROR("Cannot bind unix socket"); abort(); }
    int res = listen(app.listenfd, 1); assert(res == 0);

    service_init(app.sv, port, key, capture, playback, (void(*)(const uint8_t*, void*))service_handler, &app);
    service_pollfd(app.sv, app.listenfd, (void(*)(void*))connection_handler, &app);
    while(1) service_wait(app.sv, -1);
}
