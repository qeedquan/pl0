#!/bin/sh

sh build.sh
for i in input/*.pl0
do
    ./pl0 $i 
    if [[ $? != 0 ]]
    then
        echo "test failed:" "$i"
        exit 1
    fi
done

echo "All test passes"
