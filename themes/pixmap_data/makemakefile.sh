#!/bin/sh

echo 'pixmapdir = $(datadir)/gtk/themes/pixmap' > Makefile.am
echo 'dummy =' >> Makefile.am
echo 'pixmap_DATA = \' >> Makefile.am

for i in *.png gtkrc ; do echo "	$i \\" >> Makefile.am ; done

echo '	$(dummy)' >> Makefile.am
echo 'EXTRA_DIST = $(pixmap_DATA)' >> Makefile.am