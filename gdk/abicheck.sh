#! /bin/sh

cpp -P -DGDK_WINDOWING_X11  ${srcdir:-.}/gdk.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' | sort | uniq > expected-abi
nm -D -g --defined-only .libs/libgdk-3.so | cut -d ' ' -f 3 | egrep -v '^(__bss_start|_edata|_end)' | sort > actual-abi
diff -u expected-abi actual-abi && rm -f expected-abi actual-abi
