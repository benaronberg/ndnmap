#!/bin/bash

#TO DO
#get nfd-start working (can't obtain sudo privelages)
#will probably need a script to nfdc register each connection (is there a better way? ask haowei)

CWD=`pwd`

source ~/.topology
source hosts
source routers
source helperFunctions

# ROUTER_HOST_PAIRS contains 'tuples' of
#  router-hosts pair names/prefixes. There can be 
#  duplicate routers but not hosts
echo "start nfd on all machines"

started_nfd=()
for s in "${ROUTER_HOST_PAIRS[@]}" 
do
  pair_info=(${s//:/ })
  ROUTER=${pair_info[0]}
  HOST=${pair_info[1]}
  echo "startAll.sh, nfd: $ROUTER, $HOST"
  # array_contains defined in helperFunctions
  if ! array_contains $started_nfd $ROUTER
  then
    # start nfd on ROUTER
    ssh ${!ROUTER} "cd $CWD ; ./start_nfd.sh"
    started_nfd+=("$ROUTER")
  fi
  # start nfd on HOST
  ssh ${!HOST} "cd $CWD ; ./start_nfd.sh" 
done


echo "Sleep so nlsr will be able to start"
sleep 10


# start nlsr on all of the routers
echo "start nlsr on routers"
echo ${ROUTER_CONFIG}
for s in "${ROUTER_CONFIG[@]}"
do
  router_info=(${s//:/ })
  HOST=${router_info[2]}
  NAME=${router_info[1]}
  echo "startAll.sh, nlsr: $NAME"
  ssh ${!HOST} "cd $CWD ; nohup nlsr -f ./NLSR_CONF/$NAME.conf > ./NLSR_OUTPUT/$NAME.OUTPUT 2>&1 &"
done

# start nfdstat_c on all machines
echo "start nfdstat_c on all machines"

INTEREST_FILTER=$1
echo "INTEREST_FILTER: $INTEREST_FILTER"
for s in "${ROUTER_HOST_PAIRS[@]}" 
do
  pair_info=(${s//:/ })
  ROUTER=${pair_info[0]}
  HOST=${pair_info[1]}
  
  echo "nfdstat_c -p $INTEREST_FILTER"
  ssh ${!ROUTER} "cd $CWD ; echo 'nfdstat_c logfile\nnfd pid: ' > nfdstat.log ; ./getNfdPid.sh >> nfdstat.log  ; ./build/nfdstat_c -p $INTEREST_FILTER -d 1 >> nfdstat.log "
  ssh ${!HOST} "cd $CWD ; echo 'nfdstat_c logfile\nnfd pid: ' > nfdstat.log ; ./getNfdPid.sh >> nfdstat.log  ; ./build/nfdstat_c -p $INTEREST_FILTER -d 1 >> nfdstat.log "
done

# start nfdstat_s on WU host
echo "start nfdstat_s"
ssh $h9x2 "cd $CWD ; echo 'nfdstat_s logfile' > nfdstatserver.log ; ./build/nfdstat_s -f linksList.6RTRs -n 12 >> nfdstatserver.log"




