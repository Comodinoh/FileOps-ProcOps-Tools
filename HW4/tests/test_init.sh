#!/bin/bash

DIRS_TO_CHECK="bin/ src/ include/ data/ logs/ reports/ tmp/ tests/ doc/ tools/"

./tools/fileops.sh init --silent

if [ $? -ne 0 ]; then
    exit 1
fi


for dir in $DIRS_TO_CHECK; do 
   if [ ! -d $dir ]; then
       exit 1
   fi
done

exit 0
