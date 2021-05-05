#! /usr/bin/env python3

import sys
import gi

gi.require_version('Gtk', '4.0')

from gi.repository import Gtk

assert isinstance(Gtk.INVALID_LIST_POSITION, int), 'Gtk.INVALID_LIST_POSITION is not an int'
