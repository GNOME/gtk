#! /bin/sh
# package_tutorial.sh - Package up the tutorial into various formats
# Copyright (C) Tony Gale 2000
# Contact: gale@gtk.org
#
# NOTE: This script requires the following to be installed:
#            o DocBook
#            o Jade
#            o Jadetex

TARGET=`pwd`/gtk-tut.sgml
JPGS="`pwd`/*.jpg"
EPSS="`pwd`/*.eps"
EXAMPLES=`pwd`/../../examples

PATH=`pwd`:$PATH

DATE=`date '+%Y%m%d'`

# Check top level directory
if [ ! -d gtk-tutorial.$DATE ]; then
  if [ -e gtk-tutorial.$DATE ]; then
    echo "ERROR: gtk-tutorial is not a directory"
    exit
  fi
  if ! mkdir gtk-tutorial.$DATE; then
    echo "mkdir failed"
    exit 1
  fi
fi 

if ! cd gtk-tutorial.$DATE; then
  echo "cd failed"
  exit 1
fi

cp $TARGET .
cp $JPGS .
cp $EPSS .

# SGML Format
echo -n "Copy SGML and images.... "
if [ ! -d sgml ]; then
  if [ -e sgml ]; then
    echo "ERROR: html is not a directory"
    exit
  fi
  mkdir sgml
fi

(cd sgml ; cp $TARGET . ; cp $JPGS .)
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

(db2html gtk-tut.sgml ; mv gtk-tut/* html ; cp $JPGS html ; rm -rf gtk-tut) > /dev/null
echo "done"

# Text, PS and DVI Format
echo -n "Formatting into Text, PS and DVI.... "
if [ ! -d ps ]; then
  if [ -e ps ]; then
    echo "ERROR: ps is not a directory"
    exit
  fi
  mkdir ps
fi

if [ ! -d txt ]; then
  if [ -e txt ]; then
    echo "ERROR: ps is not a directory"
    exit
  fi
  mkdir txt
fi

sed 's/gtk_tut_packbox1.jpg/gtk_tut_packbox1.eps/ ; s/gtk_tut_packbox2.jpg/gtk_tut_packbox2.eps/ ; s/gtk_tut_table.jpg/gtk_tut_table.eps/' gtk-tut.sgml > ps/gtk-tut.sgml
(cd ps ; db2ps gtk-tut.sgml ; ps2pdf gtk-tut.ps gtk-tut.pdf ; pdftotext gtk-tut.pdf ; mv gtk-tut.txt ../txt ; rm -f *) > /dev/null 2>&1
sed 's/gtk_tut_packbox1.jpg/gtk_tut_packbox1.eps/ ; s/gtk_tut_packbox2.jpg/gtk_tut_packbox2.eps/ ; s/gtk_tut_table.jpg/gtk_tut_table.eps/' gtk-tut.sgml > ps/gtk-tut.sgml
(cp *.eps ps ; cd ps ; db2ps gtk-tut.sgml ; rm gtk-tut.aux gtk-tut.log gtk-tut.sgml gtk-tut.tex *.eps) > /dev/null 2>&1
echo "done"

# PDF Format
echo -n "Formatting into PDF.... "
if [ ! -d pdf ]; then
  if [ -e pdf ]; then
    echo "ERROR: pdf is not a directory"
    exit
  fi
  mkdir pdf
fi

(db2pdf gtk-tut.sgml ; mv gtk-tut.pdf pdf) > /dev/null
echo "done"

# Copy examples
echo -n "Copying examples"
cp -R $EXAMPLES .
(cd examples ; make clean ; rm -rf CVS */CVS)
echo "done"

rm -f *

# Package it all up
echo -n "Creating packages.... "
cd ..
tar cvfz gtk-tutorial.$DATE.tar.gz gtk-tutorial.$DATE
echo "done"

rm -rf gtk-tutorial.$DATE

echo
echo Package gtk-tutorial.$DATE.tar.gz created.
echo
