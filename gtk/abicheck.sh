#! /bin/sh

cpp -P -DG_OS_UNIX -DGTK_WINDOWING_X11 -DINCLUDE_INTERNAL_SYMBOLS gtk.symbols | sed -e '/^$/d' | sort > expected-abi
nm -D .libs/libgtk-x11-2.0.so | grep " T " | cut -c12- | grep "^gtk" | sort > actual-abi
diff -u expected-abi actual-abi
