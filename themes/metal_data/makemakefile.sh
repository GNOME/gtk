#!/bin/sh

echo 'metaldir = $(datadir)/gtk/themes/metal' > Makefile.am
echo 'dummy =' >> Makefile.am
echo 'metal_DATA = \' >> Makefile.am

for i in *.png README gtkrc ; do echo "	$i \\" >> Makefile.am ; done

echo '	$(dummy)' >> Makefile.am
echo 'EXTRA_DIST = $(metal_DATA)' >> Makefile.am
