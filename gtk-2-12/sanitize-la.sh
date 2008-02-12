#!/bin/sh

sed "s/dependency_libs=.*/dependency_libs=''/" < $1 > $1T && mv $1T $1
