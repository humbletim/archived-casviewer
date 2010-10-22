#!/bin/bash

###
### Constants
###

TRUE=0 # Map the shell's idea of truth to a variable for better documentation
FALSE=1
LOG="`pwd`/logs/build_linux.log"

###
### Global Variables
###

WANTS_CLEAN=$TRUE
WANTS_CONFIG=$TRUE
WANTS_BUILD=$TRUE
WANTS_PACKAGE=$TRUE

###
### Helper Functions
###

if [ "$1" == "--rebuild" ] ; then
	echo "rebuilding..."
	WANTS_CLEAN=$FALSE
	WANTS_CONFIG=$FALSE
fi

###
###  Main Logic
### 

export CC=/usr/bin/gcc-4.3
export CXX=/usr/bin/g++-4.3
export CMAKE_CXX_FLAGS_RELEASE="-O3 -msse -msse2" 

pushd indra
if [ $WANTS_CLEAN -eq $TRUE ] ; then
	./develop.py -t release clean
	find . -name "*.pyc" -exec rm {} \;
fi

if [ $WANTS_CONFIG -eq $TRUE ] ; then
	mkdir -p ../logs > /dev/null 2>&1
	./develop.py -t release | tee $LOG
fi

if [ $WANTS_BUILD -eq $TRUE ] ; then
	echo "Building in progress... Check $LOG for verbose status"
	./develop.py -t release build 2>&1 | tee $LOG | grep -e "make.*Error "
	echo "Complete"
fi
popd
