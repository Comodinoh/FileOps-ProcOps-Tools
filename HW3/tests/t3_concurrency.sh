#!/bin/bash

./tools/fileops.sh clean --silent

CFLAGS="-Iinclude --std=gnu23 -Wall " ./tools/fileops.sh build --src=src --silent

rm -f ./tmp/index.db

if [ ! -x ./bin/file_indexer ]; then
    exit 1
fi

./tools/fileops.sh run -- file_indexer --db ./tmp/index.db --root . 1> /dev/null & ./tools/fileops.sh run -- file_indexer ---db ./tmp/index.db --root . 1> /dev/null & ./tools/fileops.sh run -- file_indexer --db ./tmp/index.db --root . 1> /dev/null&
wait

if [ ! -f ./tmp/index.db ]; then
    exit 1
fi

SIG=$(hexdump -n 32 -e '1/32 "%s"' ./tmp/index.db)

if [ "$SIG" != "IDX" ]; then
    exit 1
fi

STATE=$(hexdump -s 37 -n 1 -e '1/1 "%d"' ./tmp/index.db)

if [ "$STATE" != "1" ]; then
    exit 1
fi

rm -f ./tmp/procs.db

if [ ! -x ./bin/procs ]; then
    exit 1
fi

./tools/fileops.sh run -- procs --db ./tmp/procs.db --root . 1> /dev/null & ./tools/fileops.sh run -- procs ---db ./tmp/procs.db --root . 1> /dev/null & ./tools/fileops.sh run -- procs --db ./tmp/procs.db --root . 1> /dev/null&
wait

if [ ! -f ./tmp/procs.db ]; then
    exit 1
fi

SIG=$(hexdump -n 32 -e '1/32 "%s"' ./tmp/procs.db)

if [ "$SIG" != "PROC" ]; then
    exit 1
fi

STATE=$(hexdump -s 37 -n 1 -e '1/1 "%d"' ./tmp/procs.db)

if [ "$STATE" != "1" ]; then
    exit 1
fi

rm -f ./tmp/index1.db
rm -f ./tmp/index2.db

rm -rf ./tmp/tree
mkdir -p ./tmp/tree

echo "Hello, World!" > ./tmp/tree/t.txt
echo "Hello, planet earth. zipzipzipzipzip" > ./tmp/tree/mars.txt
echo "Hello, martian. glorpglorpglorpglorp" > ./tmp/tree/glorp.txt
cat <<EOF>> ./tmp/tree/glorp.txt
⠀⠀⠀⠀⠀⠀⠀⠀⣠⣤⠀⠀⠀⠀⠀⠀⠀⢠⣴⡆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⢹⣿⠃⠀⠀⠀⠀⠀⠀⠀⣹⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠸⣏⠀⠀⠀⠀⠀⠀⠀⠀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⣼⢻⣄⣀⣠⣀⣤⣤⣀⡀⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⢠⣴⢏⣾⣿⣿⣿⣿⣿⣿⣿⣷⣿⣿⣦⣄⡀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⣠⣾⣿⣿⣾⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣶⣄⡀⠀⠀⠀⠀
⠀⣤⣶⣶⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⡀⠀⠀
⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡗⠀⠀
⠀⢻⣿⣿⣿⣿⣿⣿⣿⠟⠛⠛⠛⠿⣿⣿⣿⣿⣿⣿⣿⠟⠛⠻⣿⣿⣿⠀⠀⠀
⠀⠈⣿⣿⣿⢿⣿⣿⣿⣷⣦⣤⣀⣀⣽⣿⣿⣿⣿⣿⣧⣀⣀⣤⣿⣿⣿⣇⠀⠀
⠀⠘⣿⡿⣼⢿⣯⣿⢿⣿⣿⣿⣿⣿⡯⡝⣎⢿⣻⣿⣿⣿⣿⣿⣿⣿⣿⣿⠂⠀
⠀⠀⣿⣹⣭⡟⡿⣞⡿⣯⣟⡿⣳⢷⡹⣜⠠⣈⠵⣯⣿⣿⣿⣿⣿⣿⣿⣿⠃⠀
⠀⠀⠹⣯⣝⡳⣱⢛⡷⣻⢿⢷⣛⡬⢧⡙⠦⣉⠰⣹⢿⣿⣿⣿⣿⣿⣿⣿⠀⠀
⠀⠀⠀⢷⢮⡵⢣⢯⣜⣳⢻⣞⢧⡛⠴⣉⠰⣀⢆⡱⣺⣿⣿⣿⣿⣿⣿⣿⡇⠀
⠀⠀⠀⣾⡑⡎⣝⠲⡜⢆⡳⠜⣎⠿⣳⢭⡳⡵⢮⣷⣿⣿⣿⣿⣿⣿⣿⣿⣿⡀
⠀⠀⠼⠣⠜⠱⠌⠓⠜⠣⠜⠱⠈⠆⠡⠊⠱⠉⠗⠺⠳⠿⠿⠿⠿⠿⠿⠿⠿⠧

EOF

ln -s glorp.txt tmp/tree/kbitty.txt

./bin/file_indexer --root tmp/tree --db tmp/index1.db 1> /dev/null


rm -rf ./tmp/tree/*
echo "what the... what is going on here!" > ./tmp/tree/t.txt
echo "Dont worry about it" > ./tmp/tree/mars.txt
echo "okey! yay!" >> ./tmp/tree/t.txt
echo "glorpglorpglorp the plan is going well glorp glorpglrop" > ./tmp/tree/glorp.txt
cat <<EOF>> ./tmp/tree/glorp.txt
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░░██░░░░░░░░░░░░░░███░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░████░░░░░░░░░░░░░██░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░█▓█░░░░░░░░░░░░░░░█░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░▒▓█░░░░░░░░░░░░░░░█░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░█▓█░░░░░░░░░░░░░░██░░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░█▓█░░░░░░░░░░░░░░███░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░░░░█▓█████████████░░███░░░░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░░░█████▓███████████████▓████░░░░░░░░░░░░░░░░░░░░░
░░░░░░░░░░█████▓█▓▓█▓██▓█▓████████████████░░░░░░░░░░░░░░░░░
░░░░░░░░███████▓█▓▓█▓▓███████▓██████████████░░░░░░░░░░░░░░░
░░░░░░░▓███▓▓▓▓▒▓█▓▒█▓▓▒█▒▓████▓███████████████░░░░░░░░░░░░
░░▒███████▓▓▓▓▓▓██▓██▒▒▓██▓█▓███▓█▓██████████████░░░░░░░░░░
░░██▓▒▒▒▒▓█▓▓▓▓▒▒▓▓██▓▓▒██▓████▓██████████████████░░░░░░░░░
░░██▓▓██▒▓▓▓▓▓████████▒░▒████████████▓▒███████████░░░░░░░░░
░░██▒▒▓▓████▓█████████████▓▓▓████████████████████░░░░░░░░░░
░░░█▓▒▓▓▓▓▓▒▒░░░░░░░░░░░██▓██▓████████░░░░▒█████░░░░░░░░░░░
░░░░█▒▓█▓▓▓▓███████░░░░░░░█▓█▒▓██████░░░░░▓██████░░░░░░░░░░
░░░░███▓▒▓▓▓▒█▓█████████▒▓▒▒██▓████▒▓███████▓▓████░░░░░░░░░
░░░░█▓▓▓▒▒▒▓▒▒▒▓▓▓████████▒▓▒░▒▓▓█████████▒▓██████░░░░░░░░░
░░░░█▓▒▒▒▒▓▓▓▓▓▓▒▒▒▓▒▒▓▓▒▓▓▒░░▓▒█▓█████████▓▒▓████░░░░░░░░░
░░░░█▓▒▓▒▓▓██▓▓▓▓▓▓▓▒▒▒▒▓▒▓▓░░░░░░░░▓███▒▒▓▓▓█████░░░░░░░░░
░░░░██▒▒▓▓▒░▒▒▒▒▓▓▓▓▓▒▓▓▒▒▒░▓▓▒░░▓████████████████░░░░░░░░░
░░░░░█▓▒▒▒▒▒▒▒▒▒▓█▓▒▓▓░░▒▓▒▒▒▒▓▓░░▒▒░▒▒▒██▒█▓▓███▒░░░░░░░░░
░░░░░▒▓▓▒▓▒▒▒▓▒▒▒▒▒▓▓▓█▓▒▒▒▒░░░░░░░░░░░█████▓█████░░░░░░░░░
░░░░░░██▓▓▒▓▓▓▓▒▒▒▓▒▒▒▒▒▒░░░░░▒░▒░░░░▓▓███████████░░░░░░░░░
░░░░░░▓▓▒▒▒▒▓▒▒▓▒▒▒▒▓▓▓▓▓▓█▓▓▓▒▒▓▓██████▓▓▓████████░░░░░░░░
░░░░░▓█▒▒▒▒░▒▒▒▒▒▒▒▒▒░▓▓▓▓█▓█████▓█▓▒░▒▒▓▓██████████░░░░░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░▒▒▒▒▓▓███████░░░░░░

EOF

ln -s mars.txt tmp/tree/kbitty.txt

./bin/file_indexer --root tmp/tree --db tmp/index2.db 1> /dev/null

./tools/fileops.sh run -- db_diff --old tmp/index1.db --new tmp/index2.db --out reports/T3_filediff.txt 1> /dev/null

rm -f ./tmp/procs1.db
rm -f ./tmp/procs2.db

./bin/procs --db tmp/procs1.db
sleep 10 & (sleep 2 && ./bin/procs --db tmp/procs2.db) & wait

./bin/db_diff --old tmp/procs1.db --new tmp/procs2.db --out reports/T3_procdiff.txt 1> /dev/null

exit 0


