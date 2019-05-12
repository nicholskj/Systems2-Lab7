#!/bin/bash
#
# driver.sh - This is a simple autograder for the Proxy Lab. It does
#     basic sanity checks that determine whether or not the code
#     behaves like a caching proxy. 
#
#     David O'Hallaron, Carnegie Mellon University
#     updated: 2/8/2016
#     updated: 4/21/2019 by C. Norris
# 
#     usage: ./driver.sh
# 

# Point values
MAX_BASIC=40
MAX_CACHE=40

# Various constants
HOME_DIR=`pwd`
PROXY_DIR="./.proxy"
NOPROXY_DIR="./.noproxy"
TIMEOUT=5
MAX_RAND=63000
PORT_START=1024
PORT_MAX=65000
MAX_PORT_TRIES=10

# List of text and binary files for the basic test
# These files must be in the ./tiny directory or the script will fail
BASIC_LIST=("home.html" "csapp.c" "godzilla.gif" "tiny.c" 
            "csapp.h" "godzilla.jpg" "whatIDo.png" "cgi-bin/adder?103&14")

BASIC_LIST_OUT=("home.html" "csapp.c" "godzilla.gif" "tiny.c" 
                "csapp.h" "godzilla.jpg" "whatIDo.png" "adder")

# List of text files for the cache test
CACHE_LIST=("home.html" "csapp.c" "csapp.h" "godzilla.gif" 
            "csapp.c" "godzilla.jpg" "whatIDo.png" "tiny.c"
            "cgi-bin/adder?103&14")
CACHE_LIST_OUT=("home.html" "csapp.c" "csapp.h" "godzilla.gif" 
                "csapp.c" "godzilla.jpg" "whatIDo.png" "tiny.c"
                "adder")

# After the accesses above, these files should
# be in the cache
CACHED=("csapp.c" "godzilla.jpg" "godzilla.gif" "tiny.c" "cgi-bin/adder?103&14")
CACHED_OUT=("csapp.c" "godzilla.jpg" "godzilla.gif" "tiny.c" "adder")

# After the accesses above, these files should
# NOT be in the cache because of size or the eviction
NOTCACHED=("home.html" "csapp.h" "whatIDo.png")
NOTCACHED_OUT=("home.html" "csapp.h" "whatIDo.png")

#####
# Helper functions
#

#
# download_proxy - download a file from the origin server via the proxy
# usage: download_proxy <testdir> <filename> <origin_url> <proxy_url>
#
function download_proxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --proxy $4 --output $2 $3
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# download_noproxy - download a file directly from the origin server
# usage: download_noproxy <testdir> <filename> <origin_url>
#
function download_noproxy {
    cd $1
    curl --max-time ${TIMEOUT} --silent --output $2 $3 
    (( $? == 28 )) && echo "Error: Fetch timed out after ${TIMEOUT} seconds"
    cd $HOME_DIR
}

#
# clear_dirs - Clear the download directories
#
function clear_dirs {
    rm -rf ${PROXY_DIR}/*
    rm -rf ${NOPROXY_DIR}/*
}

#
# wait_for_port_use - Spins until the TCP port number passed as an
#     argument is actually being used. Times out after 5 seconds.
#
function wait_for_port_use() {
    timeout_count="0"
    portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
        | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
        | grep -E "[0-9]+" | uniq | tr "\n" " "`

    echo "${portsinuse}" | grep -wq "${1}"
    while [ "$?" != "0" ]
    do
        timeout_count=`expr ${timeout_count} + 1`
        if [ "${timeout_count}" == "${MAX_PORT_TRIES}" ]; then
            kill -ALRM $$
        fi

        sleep 1
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`
        echo "${portsinuse}" | grep -wq "${1}"
    done
}


#
# free_port - returns an available unused TCP port 
#
function free_port {
    # Generate a random port in the range [PORT_START,
    # PORT_START+MAX_RAND]. This is needed to avoid collisions when many
    # students are running the driver on the same machine.
    port=$((( RANDOM % ${MAX_RAND}) + ${PORT_START}))

    while [ TRUE ] 
    do
        portsinuse=`netstat --numeric-ports --numeric-hosts -a --protocol=tcpip \
            | grep tcp | cut -c21- | cut -d':' -f2 | cut -d' ' -f1 \
            | grep -E "[0-9]+" | uniq | tr "\n" " "`

        echo "${portsinuse}" | grep -wq "${port}"
        if [ "$?" == "0" ]; then
            if [ $port -eq ${PORT_MAX} ]
            then
                echo "-1"
                return
            fi
            port=`expr ${port} + 1`
        else
            echo "${port}"
            return
        fi
    done
}


#######
# Main 
#######

######
# Verify that we have all of the expected files with the right
# permissions
#

# Kill any stray proxies or tiny servers owned by this user
killall -q proxy tiny nop-server.py 2> /dev/null

# Make sure we have a tiny directory
if [ ! -d ./tiny ]
then 
    echo "Error: ./tiny directory not found."
    exit
fi

# If there is no tiny executable, then try to build it
if [ ! -x ./tiny/tiny ]
then 
    echo "Building the tiny executable."
    (cd ./tiny; make)
    echo ""
fi

# Make sure we have all the tiny files we need
if [ ! -x ./tiny/tiny ]
then 
    echo "Error: ./tiny/tiny not found or not an executable file."
    exit
fi

# Make sure we have an existing executable proxy
if [ ! -x ./proxy ]
then 
    echo "Error: ./proxy not found or not an executable file. Please rebuild your proxy and try again."
    exit
fi

# Make sure we have an existing executable nop-server.py file
if [ ! -x ./nop-server.py ]
then 
    echo "Error: ./nop-server.py not found or not an executable file."
    exit
fi

# Create the test directories if needed
if [ ! -d ${PROXY_DIR} ]
then
    mkdir ${PROXY_DIR}
fi

if [ ! -d ${NOPROXY_DIR} ]
then
    mkdir ${NOPROXY_DIR}
fi

# Add a handler to generate a meaningful timeout message
trap 'echo "Timeout waiting for the server to grab the port reserved for it"; kill $$' ALRM

#####
# Basic
#
echo "*** Basic ***"

# Run the tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on ${tiny_port}"
cd ./tiny
./tiny ${tiny_port}   &> /dev/null  &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on ${proxy_port}"
./proxy ${proxy_port}  &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"


# Now do the test by fetching some text and binary files directly from
# tiny and via the proxy, and then comparing the results.
basicScore=0
numRun=1
for ((i=0;i<${#BASIC_LIST[@]};++i))
do
    file=${BASIC_LIST[i]}
    outfile=${BASIC_LIST_OUT[i]}
    echo "${numRun}: ${file}"
    numRun=`expr ${numRun} + 1`
    clear_dirs

    # Fetch using the proxy
    echo "   Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    download_proxy $PROXY_DIR ${outfile} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"

    # Fetch directly from tiny
    echo "   Fetching ./tiny/${file} into ${NOPROXY_DIR} directly from tiny"
    download_noproxy $NOPROXY_DIR ${outfile} "http://localhost:${tiny_port}/${file}"

    # Compare the two files
    echo "   Comparing the two files"
    diff -q ${PROXY_DIR}/${outfile} ${NOPROXY_DIR}/${outfile} &> /dev/null
    if [ $? -eq 0 ]; then
        basicScore=`expr ${basicScore} + 5`
        echo "   Success: Files are identical."
    else
        echo "   Failure: Files differ."
    fi
done

echo "Killing tiny and proxy"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "basicScore: $basicScore/${MAX_BASIC}"

if [ "$basicScore" != "${MAX_BASIC}" ]; then
   echo "Some basic tests failed. Halting execution."
   exit
fi

#####
# Caching
#
echo ""
echo "*** Cache ***"

# Run the tiny Web server
tiny_port=$(free_port)
echo "Starting tiny on port ${tiny_port}"
cd ./tiny
./tiny ${tiny_port} &> /dev/null &
tiny_pid=$!
cd ${HOME_DIR}

# Wait for tiny to start in earnest
wait_for_port_use "${tiny_port}"

# Run the proxy
proxy_port=$(free_port)
echo "Starting proxy on port ${proxy_port}"
./proxy ${proxy_port} &> /dev/null &
proxy_pid=$!

# Wait for the proxy to start in earnest
wait_for_port_use "${proxy_port}"

# Fetch some files from tiny using the proxy
clear_dirs
echo "Request files to test out the caching and the LRU policy."
for ((i=0;i<${#CACHE_LIST[@]};++i))
do
    file=${CACHE_LIST[i]}
    outfile=${CACHE_LIST_OUT[i]}
    echo "   Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    download_proxy $PROXY_DIR ${outfile} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
done

# Kill tiny
echo "Killing tiny"
kill $tiny_pid 2> /dev/null
wait $tiny_pid 2> /dev/null

# Now make sure that the files that should be cached are.
echo -e "\nRequest files that should be in cache."
cacheScore=0
numRun=1
for ((i=0;i<${#CACHED[@]};++i))
do
    file=${CACHED[i]}
    outfile=${CACHED_OUT[i]}
    echo "${numRun}: ${file}"
    numRun=`expr ${numRun} + 1`
    echo "   Fetching ./tiny/${file} into ${NOPROXY_DIR} using the proxy"
    download_proxy $NOPROXY_DIR ${outfile} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
    diff -q ${PROXY_DIR}/${outfile} ${NOPROXY_DIR}/${outfile}  &> /dev/null
    if [ $? -eq 0 ]; then
        cacheScore=`expr ${cacheScore} + 5`
        echo "   Success: Was able to fetch tiny/${file} from the cache."
    else
        echo "   Failure: Was not able to fetch tiny/${file} from the cache."
    fi
done

echo -e "\nRequest files that should NOT be in cache."
for ((i=0;i<${#NOTCACHED[@]};++i))
do
    file=${NOTCACHED[i]}
    outfile=${NOTCACHED_OUT[i]}
    echo "${numRun}: ${file}"
    numRun=`expr ${numRun} + 1`
    echo "   Fetching ./tiny/${file} into ${NOPROXY_DIR} using the proxy"
    download_proxy $NOPROXY_DIR ${outfile} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
    diff -q ${PROXY_DIR}/${outfile} ${NOPROXY_DIR}/${outfile}  &> /dev/null
    if [ $? -eq 0 ]; then
        echo "   Failure: Was able to fetch tiny/${file} from the cache."
    else
        cacheScore=`expr ${cacheScore} + 5`
        echo "   Success: Was not able to fetch tiny/${file} from the cache."
    fi
done

# Kill the proxy
echo "Killing proxy"
kill $proxy_pid 2> /dev/null
wait $proxy_pid 2> /dev/null

echo "cacheScore: $cacheScore/${MAX_CACHE}"

# Emit the total score
totalScore=`expr ${basicScore} + ${cacheScore}`
maxScore=`expr ${MAX_BASIC} + ${MAX_CACHE}`
echo ""
echo "totalScore: ${totalScore}/${maxScore}"
exit

