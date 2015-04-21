#!/usr/bin/python
#
# This is a simple systray applet for the dvs daemon.
# Config file `routes`, format is "<ID> <IP>:<PORT>\n" per route.
# Config file `contacts`, format is "<ID> <NAME>\n" per contact.
#

import sys, time, subprocess, socket, gobject, gtk

sockpath = '/tmp/dvs'
for arg in sys.argv:
    if '--socket' in arg: sockpath = arg.split('=')[1]
proc = subprocess.Popen(['dvsd'] + sys.argv[1:])
time.sleep(.1)

class Application:
    def __init__(self, sockpath):
        self.dialog = None

        # Socket
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.connect(sockpath)
        with open('routes') as f: self.sock.send(''.join('ADD %s\n' % s for s in f.read().splitlines()))
        gobject.io_add_watch(self.sock, gobject.IO_IN, self.receive)

        # Left click menu
        self.contacts = {}
        self.lbmenu = gtk.Menu()
        with open('contacts') as f:
            for line in f.read().splitlines():
                id, name = line.split(' ', 1)
                item = gtk.MenuItem(name)
                item.connect('activate', self.dial, id)
                self.lbmenu.append(item)
                self.contacts[id] = name
        self.lbmenu.show_all()

        # Right click menu
        self.rbmenu = gtk.Menu()
        item = gtk.ImageMenuItem(gtk.STOCK_QUIT)
        item.connect('activate', self.quit)
        self.rbmenu.append(item)
        self.rbmenu.show_all()

        # Status icon
        self.tray = gtk.StatusIcon()
        self.tray.connect('activate', self.lbclick)
        self.tray.connect('popup-menu', self.rbclick)
        self.set_state(0)

    def set_state(self, active):
        self.active = active
        self.tray.set_tooltip('Hangup' if active else 'Dial')
        self.tray.set_from_file('/usr/share/icons/gnome/scalable/actions/call-%s-symbolic.svg' % ('end' if active else 'start'))

    def dial(self, item, id):
        self.sock.send('DIAL %s\n' % id)
        self.set_state(1)

    def hangup(self):
        self.sock.send('HANGUP\n')
        self.set_state(0)

    def answer(self, dialog, response):
        self.dialog = dialog.destroy()
        if self.active:
            if response == gtk.RESPONSE_YES:
                self.sock.send('ANSWER\n')
            else:
                self.sock.send('HANGUP\n')
                self.set_state(0)

    def receive(self, source, cond):
        for msg in self.sock.recv(128).splitlines():
            if 'HANGUP' in msg:
                self.set_state(0)
                if self.dialog: self.dialog = self.dialog.destroy()
            elif 'RING' in msg:
                id = msg.split()[1]
                self.set_state(1)
                self.dialog = gtk.MessageDialog(type = gtk.MESSAGE_QUESTION, buttons = gtk.BUTTONS_YES_NO)
                self.dialog.set_markup('Answer call from `%s` ?' % (self.contacts[id] if id in self.contacts else 'unknown'))
                self.dialog.connect('response', self.answer)
                self.dialog.show()
        return True

    def lbclick(self, tray):
        if self.active: self.hangup()
        else: self.lbmenu.popup(None, None, gtk.status_icon_position_menu, 0, gtk.get_current_event_time(), tray)

    def rbclick(self, tray, button, time):
        self.rbmenu.popup(None, None, gtk.status_icon_position_menu, button, time, tray)

    def quit(self, item):
        self.sock.close()
        gtk.mainquit()

try:
    Application(sockpath)
    gtk.main()
    time.sleep(.1)
finally:
    proc.kill()
