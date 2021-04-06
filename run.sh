#!/bin/bash
mkdir -p outputs
for file in tests/*.txt; do
  filename=$(basename "$file")
  echo ========== TEST "$filename" ==========
  cat tests/"$filename"
  ./dce < tests/"$filename" > outputs/"$filename"
  cat outputs/"$filename"
done
