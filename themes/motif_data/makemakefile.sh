#!/bin/sh

echo 'motifdir = $(datadir)/gtk/themes/motif' > Makefile.am
echo 'dummy =' >> Makefile.am
echo 'motif_DATA = \' >> Makefile.am

for i in *.png README gtkrc ; do echo "	$i \\" >> Makefile.am ; done

echo '	$(dummy)' >> Makefile.am
echo 'EXTRA_DIST = $(motif_DATA)' >> Makefile.am
