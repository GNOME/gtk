#!/bin/bash

REF="main"

# project name followed by CI job name
# GLib needs platform-specific docs to be downloaded, hence one job on Windows and one on Linux
PROJECTS=( \
	"gtk reference" \
        "gdk-pixbuf reference" \
	"glib msys2-mingw32" \
	"glib fedora-x86_64" \
)

for PAIR in "${PROJECTS[@]}"; do
	# shellcheck disable=SC2086
	set -- $PAIR
	PROJECT="$1"
	JOB_NAME="$2"

	curl -L --output "$REF-docs.zip" "https://gitlab.gnome.org/GNOME/$PROJECT/-/jobs/artifacts/$REF/download?job=$JOB_NAME" || exit $?
	unzip -o -d "$REF-docs" "$REF-docs.zip" || exit $?
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
glib-unix
glib-win32
gmodule
gobject
gio
gio-unix
gio-win32
girepository
"

IFS='
'

for SECTION in $SECTIONS; do
        mv "$REF-docs/_reference/$SECTION" "$DOCS_DIR/$SECTION"
done

rm -rf "$REF-docs"
