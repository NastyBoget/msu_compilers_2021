#!/bin/bash
mkdir -p outputs
for ((i=1; i<=5; i++))
do
echo ========== TEST $i ==========
cat tests/$i.txt
./dce < tests/$i.txt > outputs/$i.txt
cat outputs/$i.txt
done
