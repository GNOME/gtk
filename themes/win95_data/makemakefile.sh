#!/bin/sh

echo 'win95dir = $(datadir)/gtk/themes/win95' > Makefile.am
echo 'dummy =' >> Makefile.am
echo 'win95_DATA = \' >> Makefile.am

for i in *.png README gtkrc ; do echo "	$i \\" >> Makefile.am ; done

echo '	$(dummy)' >> Makefile.am
echo 'EXTRA_DIST = $(win95_DATA)' >> Makefile.am
