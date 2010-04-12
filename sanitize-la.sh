#!/bin/sh

if [ -f $1 ]; then
    sed "s/dependency_libs=.*/dependency_libs=''/" < $1 > $1T && mv $1T $1
fi

