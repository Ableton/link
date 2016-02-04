#!/bin/bash
config="Debug"
linking="static"

./modules/build-system/scripts/configure.py -l $linking
./modules/build-system/scripts/build.py -c $config -l $linking -t LinkDiscoveryTest "$@"
./modules/build-system/scripts/build.py -c $config -l $linking -t LinkCoreTest "$@"
./modules/build-system/scripts/run.py cpptest -c $config -l $linking
