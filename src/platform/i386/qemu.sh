#!/bin/sh
if [ $# -gt 2 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi

if [ $# -eq 1 ]; then
MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')
elif [ $# -eq 2 ]; then
MODULES=$(sh $1 $2 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')
fi


qemu-system-i386 -m 768 -nographic -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"

