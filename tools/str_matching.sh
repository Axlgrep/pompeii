#!/bin/bash
array=(`ls -l write2file* | awk '{print $9}'`)

for ((i=0; i<${#array[@]}; i++))
do
  num=$(cat ${array[$i]} | grep -a bcba0dc2b9ffbcb1f68e0b23fa217193 | wc -l)
  if [ "$num" != "0" ]; then
    echo ${array[$i]} $num
  fi
done
