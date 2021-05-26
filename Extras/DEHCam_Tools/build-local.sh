#!/bin/bash
mkdir src 
rm `basename $1`.bin
rm src/*.*
cp $1/*.* src
cp -r libraries/* src
make -f $PARTICLE_MAKEFILE compile-all
