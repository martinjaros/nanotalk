# Nanotalk voice client

This is a simple, distributed voice client for Linux desktop.
After `make install` two files will be installed - `nanotalk` and `ntclient`,
run `ntclient` startup script for the GUI. Python with Gtk+ support is needed; see script source for details.

There are two configuration files loaded at startup:

 * `~/.nanotalk/routes` - Contains routing information; format is `<ID> <IP>:<PORT>\n` per route
 * `~/.nanotalk/contacts` - Contains contact names; format is `<ID> <NAME>\n` per contact

Following options are supported:

    -h, --help       Print help
    -v, --version    Print version
    -p, --port       Set port number, default 5004
    -k, --key        Set key file, default `/etc/ssl/private/nanotalk.key`
    -r, --bitrate    Set bitrate, default 32000
    -i, --capture    Set ALSA capture device
    -o, --playback   Set ALSA playback device
    -d, --debug      Set trace verbosity

If building from git, use `autoreconf -i` to generate the configure script.

## LICENSE
Use with GNU General Public License, version 2
