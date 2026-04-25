#!/bin/bash

mkdir -p ./tmp/test_scenario_src/app ./tmp/test_scenario_src/lib/include

echo "#ifndef _UTIL_H_" > ./tmp/test_scenario_src/lib/include/util.h
echo "#define _UTIL_H_" >> ./tmp/test_scenario_src/lib/include/util.h
echo "int util_add(int, int);" >> ./tmp/test_scenario_src/lib/include/util.h
echo "#endif" >> ./tmp/test_scenario_src/lib/include/util.h

echo "#include \"util.h\"" > ./tmp/test_scenario_src/lib/util.c
echo "int util_add(int a, int b) {return a + b;}" >> ./tmp/test_scenario_src/lib/util.c 

echo "#include \"util.h\"" > ./tmp/test_scenario_src/app/main_demo.c
echo "#include <stdio.h>" >> ./tmp/test_scenario_src/app/main_demo.c
echo "int main(void) {" >> ./tmp/test_scenario_src/app/main_demo.c
echo "  printf(\"%d\", util_add(2, 3));" >> ./tmp/test_scenario_src/app/main_demo.c
echo "  return 0;" >> ./tmp/test_scenario_src/app/main_demo.c
echo "}" >> ./tmp/test_scenario_src/app/main_demo.c

CFLAGS="-Itmp/test_scenario_src/lib/include -std=c11 -Wall -Wextra -Werror" ./tools/fileops.sh build --src tmp/test_scenario_src

if [ ! -x ./bin/demo ]; then
    exit 1
fi

./bin/demo > ./tmp/demo_out.txt

code=$?

if [ "$(cat ./tmp/demo_out.txt)" != "5" -o $code -ne 0 ]; then
    exit 1
fi

