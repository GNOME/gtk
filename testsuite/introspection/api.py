#! /usr/bin/env python3

import sys

try:
    import gi
except ImportError:
    sys.exit(77) # skip this test, gi module is not available

gi.require_version('Gtk', '4.0')

from gi.repository import Gtk

assert isinstance(Gtk.INVALID_LIST_POSITION, int), 'Gtk.INVALID_LIST_POSITION is not an int'
