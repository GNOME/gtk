#! /bin/sh
# extract - extract C source files from GTK Tutorial
# Copyright (C) Tony Gale 1998
# Contact: gale@gtk.org
#
# extract.awk command Switches:
# -c : Just do checking rather than output files
# -f <filename> : Extract a specific file
# -d : Extract files to current directory

TUTORIAL=../docs/tutorial/gtk-tut.sgml

if [ -x /usr/bin/gawk ]; then
 gawk -f extract.awk $TUTORIAL $1 $2 $3 $4 $5
else
 if [ -x /usr/bin/nawk ]; then
  nawk -f extract.awk $TUTORIAL $1 $2 $3 $4 $5
 else
  if [ -x /usr/bin/awk ]; then
   awk -f extract.awk $TUTORIAL $1 $2 $3 $4 $5
  else 
   if [ -x /bin/awk ]; then
    awk -f extract.awk $TUTORIAL $1 $2 $3 $4 $5
   else
    echo "Can't find awk... please edit extract.sh by hand"
   fi
  fi
 fi
fi

