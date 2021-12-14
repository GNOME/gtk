#!/bin/sh

REF="main"

curl -L --output "$REF-docs.zip" "https://gitlab.gnome.org/GNOME/gtk/-/jobs/artifacts/$REF/download?job=reference"
unzip -d "$REF-docs" "$REF-docs.zip"

rm -f "$REF-docs.zip"

DOCS_DIR="docs"

mkdir -p $DOCS_DIR

SECTIONS="
gdk4
gdk4-x11
gdk4-wayland
gsk4
gtk4
Pango
PangoCairo
PangoFc
PangoFT2
PangoOT
PangoXft
gdk-pixbuf
gdk-pixdata
"

IFS='
'

for SECTION in $SECTIONS; do
        mv "$REF-docs/_reference/$SECTION" "$DOCS_DIR/$SECTION"
done

rm -rf "$REF-docs"
