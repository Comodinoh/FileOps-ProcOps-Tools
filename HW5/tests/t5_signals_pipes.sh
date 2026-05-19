#!/bin/bash

./tools/fileops.sh clean --silent
CFLAGS="-Iinclude --std=gnu23 -lcrypto" ./tools/fileops.sh build --src=src --silent

rm -rf ./tmp/*
mkdir -p ./tmp/gattino/meow/meow
echo "Miau!" > ./tmp/gattino/meow/meow/meow.txt
echo "Hello!" > ./tmp/gattino/gatorice.txt
echo "Hello!" > ./tmp/gattino/gatorice1.txt
echo "Hello!" > ./tmp/gattino/gatorice2.txt
echo "Hello!" > ./tmp/gattino/gatorice3.txt
echo "Hello!" > ./tmp/gattino/gatorice4.txt
echo "Hello!" > ./tmp/gattino/gatorice5.txt
echo "Hello!" > ./tmp/gattino/gatorice6.txt

./tools/fileops.sh run -- fileops_manager --root tmp/gattino --workers 2 --db tmp/t5.db --simulate-work-ms 20000 --pid-file tmp/manager.pid > tmp/manager.log 2>&1 &

while [ ! -f tmp/manager.pid ]; do
    sleep 0.1
done

PID=$(cat tmp/manager.pid)
if [ -z "$PID" ]; then
    exit 1
fi

kill -USR1 $PID
kill -TERM $PID

while kill -0 $PID 2>/dev/null; do
    sleep 0.05
done

if [ ! -f tmp/t5.db ]; then
    exit 1
fi

./tools/fileops.sh run -- fileops_manager --db tmp/t5.db --verify
if [ $? -ne 0 ]; then
    exit 1
fi

DUMP=$(./tools/fileops.sh run -- fileops_manager --db tmp/t5.db --dump)

function assert() {
    echo "$1" | grep -q "$2"
    if [ $? -ne 0 ]; then
        exit 1
    fi
}

assert "$DUMP" "Signature: INV"
assert "$DUMP" "Version: 1"
assert "$DUMP" "Complete: 0"

grep -q "STATUS" tmp/manager.log
if [ $? -ne 0 ]; then
    exit 1
fi

exit 0
