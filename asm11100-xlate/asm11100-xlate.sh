#!/bin/sh

USRGUIDE=
BITWIDTH=

FOUND=1
while [ $FOUND -eq 1 ];
do
  FOUND=1;
  case $1 in
    -f) shift; USRGUIDE=$1; shift; ;;
    -b) shift; BITWIDTH=$1; shift; ;;
    *) FOUND=0;
  esac
done

if [ -z "$USRGUIDE" -o -z "$BITWIDTH" ]; then
  echo "$0: -f file -b bitwidth MN0.." > /dev/stderr
  exit 1
fi

if [ ! -f "$USRGUIDE" ]; then
  echo "$0: ERROR: unable to open file $USRGUIDE" > /dev/stderr
  exit 1
fi

case $BITWIDTH in
  12) ;; # PIC16C5X
  14) ;; # PIC16CXX
  16) ;; # PIC17CXX
  *)
    echo "$0: ERROR: unknown bitwidth $BITWIDTH" > /dev/stderr
    exit 1
esac

echo "$0: using file $USRGUIDE" > /dev/stderr
echo "$0: using bitwidth $BITWIDTH" > /dev/stderr

for mn in $@
do
  MATCHES=`cat $USRGUIDE | grep -e " $mn "`
  if [ -z "$MATCHES" ]; then
    echo "$0: WARNING: unable to find $mn" > /dev/stderr
  fi
  echo "$MATCHES" | while read line
  do
    DOUBLESPACE=`echo "$line" | sed 's/  /#/g'`
    NDOUBLESPACE=`echo "$DOUBLESPACE" | grep '#' | wc -c`
    if [ $NDOUBLESPACE -gt 0 ]; then
      BITS=`echo "$DOUBLESPACE" | cut -d "#" -f 1`
      NBITS=`echo $BITS | tr -d "[:space:]" | wc -c`
      if [ $NBITS -eq $BITWIDTH ]; then
        echo $mn $BITS
      fi
    fi
  done
done
