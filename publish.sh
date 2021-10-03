#!/bin/sh

DOCSDIR=$1
STATICDIR=$2
TARGET=$3

mkdir -p ${TARGET}

mv ${DOCSDIR}/* ${TARGET}
cp -r ${STATICDIR}/* ${TARGET}
