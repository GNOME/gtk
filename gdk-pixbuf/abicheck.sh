#! /bin/sh

cpp -DINCLUDE_VARIABLES -P -DALL_FILES ${srcdir:-.}/gdk-pixbuf.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE$//' | sort > expected-abi
nm -D .libs/libgdk_pixbuf-2.0.so | grep " [BDTR] " | cut -d ' ' -f 3 | sort > actual-abi
diff -u expected-abi actual-abi && rm expected-abi actual-abi
