#! /bin/sh

cpp -P -DINCLUDE_INTERNAL_SYMBOLS gdk-pixbuf.symbols | sed -e '/^$/d' | sort > expected-abi
nm -D .libs/libgdk_pixbuf-2.0.so | grep " T " | cut -c12- | grep "^\(gdk\|pixops\)_" | sort > actual-abi
diff -u expected-abi actual-abi
