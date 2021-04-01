#!/bin/bash
mkdir -p outputs
g++ dce.cpp -L./lib -lqbe -I ./include -o dce
./dce < tests/1.txt > outputs/1.txt
./dce < tests/2.txt > outputs/2.txt
./dce < tests/3.txt > outputs/3.txt
./dce < tests/4.txt > outputs/4.txt
