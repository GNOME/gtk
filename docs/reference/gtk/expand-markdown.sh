#!/bin/sh
#
# Generate docbook from markdown
#
# Usage: expand-markdown.sh INPUT.md OUTPUT.xml

set -x
basename=$(basename $1 .md)
outputdir=$(dirname $2)
outputfile=$(basename $2)
pandoc $1 --from=gfm \
          --to=docbook \
          --id-prefix=${basename}- \
          --top-level-division=chapter \
          -o $2
cd ${outputdir}
gtkdoc-mkdb --module gtk4 --output-dir xml --expand-content-files ${outputfile}
