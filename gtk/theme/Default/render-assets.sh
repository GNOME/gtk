#! /bin/bash

INKSCAPE="/usr/bin/inkscape"
OPTIPNG="/usr/bin/optipng"

INDEX="assets.txt"
SRC_FILE="assets.svg"
ASSETS_DIR="assets"

for contrast in "" "hc-";
do

  for i in `cat $INDEX`
  do
    if [ -f $ASSETS_DIR/$contrast$i.png ]; then
      echo $ASSETS_DIR/$contrast$i.png exists.
    else
      echo
      echo Rendering $contrast$i.png
      $INKSCAPE --export-id=$contrast$i \
                --export-id-only \
                --export-type="png" \
                --export-filename=$ASSETS_DIR/$contrast$i.png \
                $SRC_FILE >/dev/null \
       && $OPTIPNG -o7 --quiet $ASSETS_DIR/$contrast$i.png
    fi
    if [ -f $ASSETS_DIR/$contrast$i@2.png ]; then
      echo $ASSETS_DIR/$contrast$i@2.png exists.
    else
      echo
      echo Rendering $contrast$i@2.png
      $INKSCAPE --export-id=$contrast$i \
                --export-dpi=180 \
                --export-id-only \
                --export-type="png" \
                --export-filename=$ASSETS_DIR/$contrast$i@2.png \
                $SRC_FILE >/dev/null \
       && $OPTIPNG -o7 --quiet $ASSETS_DIR/$contrast$i@2.png
    fi
  done
done
exit 0
