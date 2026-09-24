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
    if [ -n "$2" ]; then
      echo "FAIL CASE DATA $2"
    fi
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
/bin/echo -n "# decode hex (xxd) "
output_ascii 0 255 | xxd_decode > /dev/null
echo_pass_fail_exit $?

output_random ()
{
  head -c $1 /dev/urandom
}

run_test_plaintext()
{
  local nbytes="$1"

  /bin/echo "# Running plain tests ($nbytes)"

  # generate test sample and store data in memory
  data=`output_random $nbytes | xxd_encode`

  /bin/echo -n "# 1A. Encode (exit code) "
  encoded=`/bin/echo -n "$data" | xxd_decode | ./na4`
  test $? -eq 0 
  echo_pass_fail_exit $?

  /bin/echo -n "# 1B. Encode (output) "
  test -n "$encoded"
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 2A. Decode (exit code) "
  decoded=`/bin/echo -n "$encoded" | ./na4 -d | xxd_encode`
  test $? -eq 0 
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 2B. Decode (data match) "
  test "$decoded" == "$data"
  echo_pass_fail_exit $?

  /bin/echo -n "# 3. Decode with zero password (must fail) "
  /bin/echo -n "$encoded" | ./na4 -d -s 0 2> /dev/null > /dev/null
  test $? -ne 0 
  echo_pass_fail_exit $?
}

for f in 1 2 3 31 32 33
do
  run_test_plaintext $f
done

run_tests_empty()
{
  /bin/echo "# Running empty data tests"

  /bin/echo -n "# 1A. Plaintext encode empty data string (zero exit code) "
  encoded=`/bin/echo -n "" | ./na4`
  test $? -eq 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 1B. Plaintext encode empty data string (no output) "
  test -z "$encoded"
  echo_pass_fail_exit $? "$encoded"

  /bin/echo -n "# 2A. Plaintext decode (zero exit code) "
  decoded=`/bin/echo -n "$encoded" | ./na4 -d`
  test $? -eq 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 2B. Plaintext decode empty data string (no output) "
  test -z "$decoded"
  echo_pass_fail_exit $? "$decoded"

  /bin/echo -n "# 3A. Encode empty data and empty secret (zero exit code) "
  encoded=`/bin/echo -n "" | ./na4 -s 0 2> /dev/null`
  test $? -eq 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 3B. Encode empty data and empty secret (no output) "
  test -z "$encoded"
  echo_pass_fail_exit $? "$encoded"

  /bin/echo -n "# 4A. Decode empty data and empty secret (should fail) "
  decoded=`/bin/echo -n "" | ./na4 -d -s 0 2> /dev/null`
  test $? -ne 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 4B. Decode empty data and empty secret (no output) "
  test -z "$decoded"
  echo_pass_fail_exit $? "$decoded"

  /bin/echo -n "# 5A. Encode empty data/secret and encrypt (zero exit code) "
  encoded=`/bin/echo -n "" | ./na4 -s 0 -e 2> /dev/null`
  test $? -eq 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 5B. Encode empty data/secret and encrypt (need output) "
  test -n "$encoded"
  echo_pass_fail_exit $? "$encoded"

  /bin/echo -n "# 6A. Decode empty data/secret as encrypted (should fail) "
  decoded=`/bin/echo -n "" | ./na4 -d -s 0 -e 2> /dev/null`
  test $? -ne 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 6B. Decode empty data/secret as encrypted (no output) "
  test -z "$decoded"
  echo_pass_fail_exit $? "$decoded"

  /bin/echo -n "# 7A. Decode encoded empty data/secret (zero exit code) "
  decoded=`/bin/echo -n "$encoded" | ./na4 -d -s 0 -e 2> /dev/null`
  test $? -eq 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 7B. Decode encoded empty data/secret (no output) "
  test -z "$decoded"
  echo_pass_fail_exit $? "$decoded"
}

run_tests_empty

run_test_password() {
  local nbytes_data="$1"
  local nbytes_password="$2"
  local f="$3"

  /bin/echo "# Running tests with password ($nbytes_password) and data ($nbytes_data) [$f]"

  # generate test sample and store data in memory
  data=`output_random $nbytes_data | xxd_encode`

  # generate two sets of different passwords with same length
  password=""
  failpass=""

  while true
  do
    # generate test password and store data in memory
    password=`output_random $nbytes_password | xxd_encode`

    # generate another test password and store data in memory
    failpass=`output_random $nbytes_password | xxd_encode`

    test "$password" == "$failpass"
    if [ $? -ne 0 ]; then
      break
    fi
  done

  /bin/echo "password $password"
  /bin/echo "failpass $failpass"
  /bin/echo "data $data"

  /bin/echo -n "# 1. Encode + Encrypt "
  encoded=`( /bin/echo -n "$password" | xxd_decode; /bin/echo -n "$data" | xxd_decode ) | ./na4 $f -s $nbytes_password`
  test -n "$encoded"
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 2. Decode with correct password (should match bit-for-bit) "
  decoded=`( /bin/echo -n "$password" | xxd_decode; /bin/echo -n "$encoded" ) |  ./na4 -d $f -s $nbytes_password | xxd_encode`
  test "$decoded" == "$data"
  echo_pass_fail_exit $? "$decoded"

  /bin/echo -n "# 3. Decode with incorrect password data but same length (should fail) "
  ( /bin/echo -n "$failpass" | xxd_decode; /bin/echo -n "$encoded" ) | ./na4 -d $f -s $nbytes_password 2> /dev/null > /dev/null
  test $? -ne 0 
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 4. Decode with incorrect longer password (should fail) "
  ( /bin/echo -n "$failpass$failpass" | xxd_decode; /bin/echo -n "$encoded" ) | ./na4 -d $f -s $nbytes_password 2> /dev/null > /dev/null
  test $? -ne 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 5. Decode with shorter password but original -s (should fail) "
  ( /bin/echo -n "$failpass" | head -c 1 | xxd_decode; /bin/echo -n "$encoded" ) | ./na4 -d $f -s $nbytes_password 2> /dev/null > /dev/null
  test $? -ne 0
  echo_pass_fail_exit $? $?

  /bin/echo -n "# 6. Decode with original password but shorter -s (should fail) "
  ( /bin/echo -n "$failpass" | xxd_decode; /bin/echo -n "$encoded" ) | ./na4 -d $f -s 1 2> /dev/null > /dev/null
  test $? -ne 0 
  echo_pass_fail_exit $? $?

  # Decode of encrypted data behaves differently than plaintext
  if [ -n "$f" ]; then
    /bin/echo -n "# 7A. Decode [$f] with known failure case (stdout must remain empty) "
    empty=`( /bin/echo -n "$failpass" | xxd_decode; /bin/echo -n "$encoded" ) | ./na4 -d $f -s $nbytes_password 2>/dev/null`
    test -z "$empty"
    echo_pass_fail_exit $? "$empty"
  else
    /bin/echo -n "# 7B. Decode plaintext with known failure case (stdout should contain data) "
    decoded2=`( /bin/echo -n "$failpass" | xxd_decode; /bin/echo -n "$encoded" ) | ./na4 -d -s $nbytes_password 2>/dev/null | xxd_encode`
    test "$decoded2" == "$data"
    echo_pass_fail_exit $? "$decoded2"
  fi

  # TODO: Tamper detection (flip a byte in the payload; MAC must fail)
}

for f in 1 2 3 31 32 33
do
  for p in `seq 43`
  do
    run_test_password "$f" "$p" ""
    run_test_password "$f" "$p" -e
  done
done
