#!/bin/bash
echo "GYP build:" &&
python modules/build-system/scripts/configure.py --wordsize $WORDSIZE
python modules/build-system/scripts/build.py --wordsize $WORDSIZE --configuration $CONFIGURATION -t LinkDiscoveryTest &&
python modules/build-system/scripts/build.py --wordsize $WORDSIZE --configuration $CONFIGURATION -t LinkCoreTest &&
python modules/build-system/scripts/run.py cpptest --wordsize $WORDSIZE --configuration $CONFIGURATION &&
python modules/build-system/scripts/build.py --wordsize $WORDSIZE --configuration $CONFIGURATION -t Controller -l dll &&
make abl-link-release
