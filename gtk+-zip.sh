#!/bin/sh

# Build zipfiles for gtk+-1.3-win32-production on Win32

ZIP=/tmp/gtk+-1.3.0-`date +%Y%m%d`.zip
DEVZIP=/tmp/gtk+-dev-1.3.0-`date +%Y%m%d`.zip
cd /target

rm $ZIP
zip -r $ZIP -@ <<EOF
COPYING.LIB-2
etc/gtk
lib/libgdk-0.dll
lib/libgtk-0.dll
share/themes/Default/gtk
EOF

zip -r $ZIP lib/locale/*/LC_MESSAGES/gtk+.mo

rm $DEVZIP
zip -r $DEVZIP -@ <<EOF
COPYING.LIB-2
include/gtk
include/gdk
lib/libgdk.dll.a
lib/gdk.lib
lib/libgtk.dll.a
lib/gtk.lib
lib/gtk+
lib/pkgconfig/gdk-1.3-win32-production.pc
lib/pkgconfig/gtk+-1.3-win32-production.pc
EOF
