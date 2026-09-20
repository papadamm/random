#!/bin/sh

output_ascii ()
{
  for e in `seq $1 $2`
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

xxd_decode ()
{
  xxd -r -p -u -c 0
}

xxd_encode ()
{
  xxd -p -u -c 0
}

# test decoding the ASCII stream first, execution only test
/bin/echo -n "decode hex "
output_ascii 0 255 | xxd_decode > /dev/null
echo_pass_fail_exit $?

for e in `seq 100`
do
  data=`output_ascii $e`

  /bin/echo -n "encode execution test $e bytes "
  /bin/echo -n "$data" | xxd_decode | ./na4 > /dev/null
  echo_pass_fail_exit $?

  /bin/echo -n "encode and decode execution test $e bytes "
  /bin/echo -n "$data" | xxd_decode | ./na4 | ./na4 -d > /dev/null
  echo_pass_fail_exit $?

  /bin/echo -n "encode and decode data test $e bytes "
  result=`/bin/echo -n "$data" | xxd_decode | ./na4 | ./na4 -d | xxd_encode`
  #/bin/echo "reference = $data"
  #/bin/echo "result = $result"
  test "$data" == "$result"
  echo_pass_fail_exit $?
done
