#!/bin/sh

xgettext --default-domain=gtk+ --directory=.. \
  --add-comments --keyword=_ --keyword=N_ \
  --files-from=./POTFILES.in \
&& test ! -f gtk+.po \
   || ( rm -f ./gtk+.pot \
    && mv gtk+.po ./gtk+.pot )
