#!/bin/sh
# simple test to go through 256 bytes of input data for each encoder/decoder
#
# TODO to harden and extend test coverage:
# - test uneven input data to the decoders
# - test uneven input data lengths to trigger 54 dual mode encoder errors
# - test robustness of single mode CRC by emulating character drop on the link
# - consider testing the single mode OOB data
# - for the msv portion some characters might not end up being used, test those

hex_tests="encode-x16:decode-xX16 encode-X16:decode-xX16"
fivefour_single_tests="encode-54s:decode-54s encode-54s:decode-54ds"
fivefour_dual_tests="encode-54d:decode-54d encode-54d:decode-54ds"

output_256_ascii ()
{
    for e in 0 `seq 255`
    do
        echo 0 `bc -l -e "hex($e)"` | tr -d "[:space:]" | tail -c 2
    done
}

echo_pass_fail_exit ()
{
    if [ $1 -eq 0 ]; then
        echo PASS
    else
        echo FAIL
        exit 1
    fi
}

# test decoding the ASCII stream first, execution only test
/bin/echo -n "decode-xX16 "
output_256_ascii | ./1654 decode-xX16 > /dev/null
echo_pass_fail_exit $?

# test encoding the binary data back to ASCII, execution only test
/bin/echo -n "encode-X16 "
output_256_ascii | ./1654 decode-xX16 | ./1654 encode-X16 > /dev/null
echo_pass_fail_exit $?

reference_output=`output_256_ascii`

# test both hex decoder and encoder together, compare results
/bin/echo -n "decode-xX16:encode-X16 "
test_output=`output_256_ascii | ./1654 decode-xX16 | ./1654 encode-X16`
test "$test_output" == "$reference_output"
echo_pass_fail_exit $?

output_256_bin ()
{
    /bin/echo -n "$reference_output" | ./1654 decode-xX16
}

for m in $hex_tests $fivefour_single_tests $fivefour_dual_tests
do
    /bin/echo -n "$m "
    
    e=`echo $m | cut -d ":" -f 1`
    d=`echo $m | cut -d ":" -f 2`
    test_output=`output_256_bin | ./1654 $e | ./1654 $d | ./1654 encode-X16`

    test "$test_output" == "$reference_output"
    echo_pass_fail_exit $?
done
