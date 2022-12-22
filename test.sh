#!/bin/bash

MY_IP=$(curl -s http://ipecho.net/plain)
TEJO="nc tejo 59000"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

run_server () {
  ./GS word_eng.txt > tests/tmp/server.log
}

run_command() {
    echo "Testing $1"
    FILE_NAME=$(basename $1 .txt)
    TEST=$(echo $FILE_NAME | cut -d'_' -f2)
    COMMAND=$(echo "$MY_IP 58046 $TEST" | $TEJO > tests/tmp/report-$TEST.html)
    if [ ! -s tests/tmp/report-$TEST.html ] || grep -q "color=\"red\"" tests/tmp/report-$TEST.html; then
        echo -e "${RED}Test failed${NC}"
    else
        echo -e "${GREEN}Test passed${NC}"
    fi
}

handle_test() {
    run_server &
    run_command $1
    killall GS
    sleep 3 # to avoid being timed out ;-;
}

# Script core

echo "You can find the results of each test in the tests/tmp folder"

# create the tests/tmp folder if it doesn't exist
mkdir -p tests/tmp
# clear it
rm -rf tests/tmp/*

for test in tests/scripts/*.txt
do
    handle_test $test
done