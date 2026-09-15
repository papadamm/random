#!/bin/sh
# simple test to go through 256 bytes of input data for each encoder/decoder
#
# TODO to harden and extend test coverage:
# - test robustness of dual mode CRC by emulating character drop on the link
# - consider testing the dual mode OOB data
# - for the msv portion some characters might not end up being used, test those

hex_tests="encode-x16:decode-xX16 encode-X16:decode-xX16"
fivefour_single_tests="encode-54s:decode-54s encode-54s:decode-54ds"
fivefour_dual_tests="encode-54d:decode-54d encode-54d:decode-54ds"

output_256 ()
{
  for e in 0 `seq 255`
  do
    echo -ne `echo -ne 0; bc -l -e "hex($e)"` | tail -c 2
  done
}

reference_output=`output_256`

for m in $hex_tests $fivefour_single_tests $fivefour_dual_tests
do
    e=`echo $m | cut -d ":" -f 1`
    d=`echo $m | cut -d ":" -f 2`
    test_output=`output_256 | ./1654 $e | ./1654 $d`
    if [ "$test_output" == "$reference_output" ]; then
	echo $m PASS
    else
	echo $m FAIL
    fi
done
