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
IMAGES="`pwd`/images"
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
cp -R $IMAGES .

# SGML Format
echo -n "Copy SGML and images.... "
if [ ! -d sgml ]; then
  if [ -e sgml ]; then
    echo "ERROR: html is not a directory"
    exit
  fi
  mkdir sgml
fi

(cd sgml ; cp $TARGET . ; cp -R $IMAGES . ; rm -rf images/CVS)
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

(db2html gtk-tut.sgml ; mv gtk-tut/* html ; cp -R $IMAGES html ; rm -rf gtk-tut) > /dev/null
(cd html ; ln -s book1.html index.html ; rm -rf images/CVS)
echo "done"

# PS, PDF and DVI Format
echo -n "Formatting into PS, DVI and PDF.... "
if [ ! -d ps ]; then
  if [ -e ps ]; then
    echo "ERROR: ps is not a directory"
    exit
  fi
  mkdir ps
fi

if [ ! -d pdf ]; then
  if [ -e pdf ]; then
    echo "ERROR: pdf is not a directory"
    exit
  fi
  mkdir pdf
fi

#sed 's/gtk_tut_packbox1.jpg/gtk_tut_packbox1.eps/ ; s/gtk_tut_packbox2.jpg/gtk_tut_packbox2.eps/ ; s/gtk_tut_table.jpg/gtk_tut_table.eps/' gtk-tut.sgml > ps/gtk-tut.sgml
sed "s/images\/\(.*\)\.png/images\/\1.eps/g" gtk-tut.sgml > ps/gtk-tut.sgml
cp -R ../images ps
(cd ps ; db2dvi gtk-tut.sgml ; dvips gtk-tut.dvi -o gtk-tut.ps ; dvipdf gtk-tut.dvi ../pdf/gtk-tut.pdf) > /dev/null 2>&1
#sed 's/gtk_tut_packbox1.jpg/gtk_tut_packbox1.eps/ ; s/gtk_tut_packbox2.jpg/gtk_tut_packbox2.eps/ ; s/gtk_tut_table.jpg/gtk_tut_table.eps/' gtk-tut.sgml > ps/gtk-tut.sgml
#sed "s/images\/\(.*\)\.png/images\/\1.eps/g" gtk-tut.sgml > ps/gtk-tut.sgml
#cp -R images ps
(cd ps ; rm gtk-tut.aux gtk-tut.log gtk-tut.sgml gtk-tut.tex ; rm -Rf images) > /dev/null 2>&1
echo "done"

# PDF Format
#echo -n "Formatting into PDF.... "
#if [ ! -d pdf ]; then
#  if [ -e pdf ]; then
#    echo "ERROR: pdf is not a directory"
#    exit
#  fi
#  mkdir pdf
#fi

#(db2pdf gtk-tut.sgml ; mv gtk-tut.pdf pdf) > /dev/null
#echo "done"

# RTF Format
echo -n "Formatting into RTF.... "
if [ ! -d rtf ]; then
  if [ -e rtf ]; then
    echo "ERROR: rtf is not a directory"
    exit
  fi
  mkdir rtf
fi

(db2rtf gtk-tut.sgml ; mv gtk-tut.rtf rtf) > /dev/null
cp -R $IMAGES rtf
echo "done"

# Copy examples
echo -n "Copying examples"
cp -R $EXAMPLES .
(cd examples ; make clean ; rm -rf CVS */CVS */.cvsignore README.1ST extract.awk extract.sh find-examples.sh)
echo "done"

rm -f *
rm -rf images

# Package it all up
echo -n "Creating packages.... "
cd ..
tar cvfz gtk-tutorial.$DATE.tar.gz gtk-tutorial.$DATE
echo "done"

rm -rf gtk-tutorial.$DATE

echo
echo Package gtk-tutorial.$DATE.tar.gz created.
echo
