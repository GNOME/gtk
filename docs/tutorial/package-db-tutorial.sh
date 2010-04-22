#! /bin/sh
# package-db-tutorial.sh - Package up the tutorial into various formats
# Copyright (C) Tony Gale 2000-2002
# Contact: gale@gtk.org
#
# NOTE: This script requires the following to be installed:
#            o DocBook
#            o DocBook-Utils
#            o Jade
#            o Jadetex

TARGETDIR=`pwd`/2.0/

SOURCE=`pwd`/gtk-tut.sgml
IMAGES="`pwd`/images"
IMAGESDIR="images"
EXAMPLES=`pwd`/../../examples

DATE=`date '+%Y%m%d'`
BUILDDIR=gtk-tutorial.$DATE

PATH=`pwd`:$PATH

# Check target directory
if [ ! -d $TARGETDIR ]; then
  if [ -e $TARGETDIR ]; then
    echo "ERROR: target directory is not a directory"
    exit
  fi
  if ! mkdir $TARGETDIR; then
    echo "mkdir for target directory failed"
    exit
  fi
fi

# Check top level build directory
if [ ! -d $BUILDDIR ]; then
  if [ -e $BUILDDIR ]; then
    echo "ERROR: build directory is not a directory"
    exit
  fi
  if ! mkdir $BUILDDIR; then
    echo "mkdir of build directory failed"
    exit 1
  fi
fi 

if ! cd $BUILDDIR; then
  echo "cd to build directory failed"
  exit 1
fi

cp $SOURCE .
cp -R $IMAGES .

# SGML Format
echo -n "Copy SGML and images.... "
if [ ! -d gtk-tutorial.sgml ]; then
  if [ -e gtk-tutorial.sgml ]; then
    echo "ERROR: gtk-tutorial.sgml is not a directory"
    exit
  fi
  mkdir gtk-tutorial.sgml
fi

(cd gtk-tutorial.sgml && cp $SOURCE . && cp -R $IMAGES . && rm -rf $IMAGESDIR/CVS)
tar cvfz $TARGETDIR/gtk-tutorial.sgml.tgz gtk-tutorial.sgml
echo "done"

# HTML Format
echo -n "Formatting into HTML.... " 
if [ ! -d gtk-tutorial.html ]; then
  if [ -e gtk-tutorial.html ]; then
    echo "ERROR: gtk-tutorial.html is not a directory"
    exit
  fi
  mkdir gtk-tutorial.html
fi

(db2html -o gtk-tutorial.html $SOURCE && cp -R $IMAGES gtk-tutorial.html && rm gtk-tutorial.html/$IMAGESDIR/*.eps) > /dev/null
(cd gtk-tutorial.html && rm -rf $IMAGESDIR/CVS && ln -s book1.html index.html)
tar cvfz $TARGETDIR/gtk-tutorial.html.tgz gtk-tutorial.html
echo "done"

# PS and PDF Format
echo -n "Formatting into PS and PDF.... "
if [ ! -d gtk-tutorial.ps ]; then
  if [ -e gtk-tutorial.ps ]; then
    echo "ERROR: gtk-tutorial.ps is not a directory"
    exit
  fi
  mkdir gtk-tutorial.ps
fi

if [ ! -d gtk-tutorial.pdf ]; then
  if [ -e gtk-tutorial.pdf ]; then
    echo "ERROR: gtk-tutorial.pdf is not a directory"
    exit
  fi
  mkdir gtk-tutorial.pdf
fi

sed "s/images\/\(.*\)\.png/images\/\1.eps/g" $SOURCE > gtk-tutorial.ps/gtk-tut.sgml
cp -R $IMAGES gtk-tutorial.ps
(cd gtk-tutorial.ps && db2dvi gtk-tut.sgml && dvips gtk-tut.dvi -o gtk-tut.ps && dvipdf gtk-tut.dvi ../gtk-tutorial.pdf/gtk-tut.pdf)
gzip -c gtk-tutorial.ps/gtk-tut.ps > $TARGETDIR/gtk-tutorial.ps.gz
gzip -c gtk-tutorial.pdf/gtk-tut.pdf > $TARGETDIR/gtk-tutorial.pdf.gz
echo "done"

# RTF Format
#echo -n "Formatting into RTF.... "
#if [ ! -d gtk-tutorial.rtf ]; then
#  if [ -e gtk-tutorial.rtf ]; then
#    echo "ERROR:  is not a directory"
#    exit
#  fi
#  mkdir gtk-tutorial.rtf
#fi

#cp -R $IMAGES gtk-tutorial.rtf
#(cd gtk-tutorial.rtf && db2rtf $SOURCE) # > /dev/null
#gzip -c gtk-tutorial.rtf/gtk-tut.rtf > $TARGETDIR/gtk-tutorial.rtf.gz
#echo "done"

# Copy examples
echo -n "Copying examples"
cp -R $EXAMPLES .
(cd examples && make clean && rm -rf CVS */CVS */.cvsignore README.1ST extract.awk extract.sh find-examples.sh)
tar cfz $TARGETDIR/examples.tgz examples
echo "done"

cd ..
rm -rf $BUILDDIR

echo
echo Packages created.
echo
