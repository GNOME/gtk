#!/bin/sh

# Build zipfile for gtk+-1.3-win32-production on Win32: both runtime
# and developer stuff

ZIP=/g/tmp/gtk+-1.3.0-`date +%Y%m%d`.zip
rm $ZIP
cd /target

zip -r $ZIP -@ <<EOF
COPYING.LIB-2
etc/gtk
include/gtk
include/gdk
lib/libgdk-0.dll
lib/libgdk.dll.a
lib/gdk.lib
lib/libgtk-0.dll
lib/libgtk.dll.a
lib/gtk.lib
lib/gtk+
lib/pkgconfig/gdk-1.3-win32-production.pc
lib/pkgconfig/gtk+-1.3-win32-production.pc
share/themes/Default/gtk
EOF

zip -r $ZIP lib/locale/*/LC_MESSAGES/gtk+.mo

