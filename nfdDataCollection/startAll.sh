#!/bin/bash

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
  echo "start_nfd.sh, nfd: $ROUTER, $HOST"
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

# start nfdstat_c on all hosts
echo "start nfdstat_c on all hosts"
INTEREST_FILTER=$1
if [ $INTEREST_FILTER -eq ""]
then
  echo "USAGE: nfdstat_c interest_filter"
fi
started_nfdstat=()
count=0
for s in "${ROUTER_HOST_PAIRS[@]}" 
do
  pair_info=(${s//:/ })
  site_info=(${ROUTER_CONFIG[$count]//:/ })
  HOST=${pair_info[1]}
  SITE=${site_info[1]}
  echo "nfdstat_c -p $INTEREST_FILTER/$SITE for host $HOST"
  ssh ${!HOST} "cd $CWD ; ./build/nfdstat_c -p $INTEREST_FILTER/$SITE -d 1 >> /dev/null 2>&1 &"
  count=$((count+1)) 
done

# start nfdstat_s on WU host
echo "start nfdstat_s"
ssh $h9x2 "cd $CWD ; ./build/nfdstat_s -f linksList.6RTRs -n 6 -l"

