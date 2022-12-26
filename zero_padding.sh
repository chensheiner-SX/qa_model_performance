#!/bin/bash

printf "Please select folder:\n\nEnter 0 to run in current folder\nEnter -1 to abort\n\n"
select d in */; do test -n "$d" && break; echo ">>> Invalid Selection"; done
cd "$d" && pwd

while true; do
  select d in */; do test -n "$d" && break; break; done
  
  if [ $REPLY -eq 0 ] || [ $REPLY -eq -1 ]; then
    break
  else
    cd "$d" && pwd
  fi
done

if [ $REPLY -eq -1 ]; then
  exit
fi

for fileType in png json; do
  a=1
  for i in $(ls *.$fileType | sort -V); do
    new=$(printf "%06d.$fileType" "$a")
    mv -i -- "$i" "$new"
    let a=a+1
  done
done
