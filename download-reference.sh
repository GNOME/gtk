#!/bin/sh

REF="main"

for PROJECT in gtk glib; do
	if [ "$PROJECT" = "glib" ]; then
		JOB_NAME="fedora-x86_64"
	else
		JOB_NAME="reference"
	fi

	curl -L --output "$REF-docs.zip" "https://gitlab.gnome.org/GNOME/$PROJECT/-/jobs/artifacts/$REF/download?job=$JOB_NAME" || exit $?
	unzip -d "$REF-docs" "$REF-docs.zip" || exit $?

	rm -f "$REF-docs.zip"
done

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
glib
gmodule
gobject
gio
"

IFS='
'

for SECTION in $SECTIONS; do
        mv "$REF-docs/_reference/$SECTION" "$DOCS_DIR/$SECTION"
done

rm -rf "$REF-docs"
