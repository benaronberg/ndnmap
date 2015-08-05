#!/bin/bash

PREFIX=$1

# start nfd on all machines
./startNfd.sh

# start nlsr on all routers
./startNlsr.sh

# configure nfd
echo "configuring routers and hosts"
./configAll.sh $PREFIX

# run data collection clients and server
./startDataCollectionScripts.sh $PREFIX
./startDataCollection.sh $PREFIX
