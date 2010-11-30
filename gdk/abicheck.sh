#! /bin/sh

cpp -P -DGDK_ENABLE_BROKEN -include ../config.h -include ./gdkconfig.h ${srcdir:-.}/gdk.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' | sort | uniq > expected-abi
nm -D -g --defined-only .libs/libgdk-x11-3.0.so | cut -d ' ' -f 3 | egrep -v '^(__bss_start|_edata|_end)' | sort > actual-abi
diff -u expected-abi actual-abi && rm -f expected-abi actual-abi
