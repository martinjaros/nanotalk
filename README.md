# Distributed voice service

This is a distributed voice service daemon.
The service listens for commands at `/tmp/dvs` by default.

Following commands are supported:
 * `ADD <ID> <ADDR>:<PORT>` - Add static route
 * `DIAL <ID>` - Dial node
 * `ANSWER` - Accept incoming call
 * `HANGUP` - Cancel call

Following responses are sent by the service:
 * `RING <ID>` - Incoming call
 * `HANGUP` - Disconnected by peer

Commands are delimited by newlines.
The ID is 20 bytes long and is formated as an uppercase hex string.
The addresses use dot-decimal notation.

Following options are supported:

    --help     Print help
    --version  Print version
    --port     Set port number, default 5004
    --key      Set key file, default `/etc/ssl/private/dvs.key`
    --socket   Set UNIX socket, default `/tmp/dvs`
    --capture  Set ALSA capture device
    --playback Set ALSA playback device
    --debug    Set trace verbosity

If building from git, use `autoreconf -i` to generate the configure script.

## LICENSE
Use with GNU General Public License, version 2
