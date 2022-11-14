#!/bin/sh
model=1
riglist=`rigctl -l | cut -c1-8 | grep -v Rig | tr -d '\n'`
for model in $riglist
do
   result=`rigctl -m $model -u 2>/dev/null | grep warnings`
#   if [[ "$result" == *"warnings: 0"* ]];then
       echo $model " " $result
#   fi

    model=`expr $model + 1`
done
