#! /bin/sh
# package_tutorial.sh - Package up the tutorial into various formats
# Copyright (C) Tony Gale 1999
# Contact: gale@gtk.org
#
# NOTE: This script requires the following to be installed:
#            o SGML Tools
#            o Latex
#            o DVI tools

TARGET=`pwd`/gtk_tut.sgml
GIFS="`pwd`/*.gif"
EXAMPLES=`pwd`/../examples

PATH=`pwd`:$PATH

DATE=`date '+%y%m%d'`

# Check top level directory
if [ ! -d gtk_tutorial ]; then
  if [ -e gtk_tutorial ]; then
    echo "ERROR: gtk_tutorial is not a directory"
    exit
  fi
  mkdir gtk_tutorial.$DATE
fi 

cd gtk_tutorial.$DATE

# SGML Format
echo -n "Copy SGML and GIF's.... "
if [ ! -d sgml ]; then
  if [ -e sgml ]; then
    echo "ERROR: html is not a directory"
    exit
  fi
  mkdir sgml
fi

(cd sgml ; cp $TARGET . ; cp $GIFS .)
echo "done"

# HTML Format
echo -n "Formatting into HTML.... " 
if [ ! -d html ]; then
  if [ -e html ]; then
    echo "ERROR: html is not a directory"
    exit
  fi
  mkdir html
fi

(cd html ; sgml2html $TARGET ; cp $GIFS .)
echo "done"

# Text Format
echo -n "Formatting into Text.... "
if [ ! -d txt ]; then
  if [ -e txt ]; then
    echo "ERROR: txt is not a directory"
    exit
  fi
  mkdir txt
fi

(cd txt ; sgml2txt -f $TARGET 2>&1 > /dev/null )
echo "done"

# PS and DVI Format
echo -n "Formatting into PS and DVI.... "
if [ ! -d ps ]; then
  if [ -e ps ]; then
    echo "ERROR: ps is not a directory"
    exit
  fi
  mkdir ps
fi

(cd ps ; sgml2latex --output=ps $TARGET > /dev/null)
(cd ps ; sgml2latex $TARGET > /dev/null)
echo "done"

# Copy examples
echo -n "Copying examples"
cp -R $EXAMPLES .
(cd examples ; make clean ; rm -rf CVS */CVS)
echo "done"

# Package it all up
echo -n "Creating packages.... "
cd ..
tar cvfz gtk_tutorial.$DATE.tar.gz gtk_tutorial.$DATE
echo "done"

rm -rf gtk_tutorial.$DATE

echo
echo Package gtk_tutorial.$DATE.tar.gz created.
echo
