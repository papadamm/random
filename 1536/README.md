# 1536 (15 <-> 36 byte Base53 encoder/decoder tool)

1536 encodes 15 bytes of raw data into 36 bytes of white space and Base53 data. It also decodes with the "-d" option. The 15 bytes input are treated as 3 separate 5 byte units that are encoded into 3 x 7 bytes Base53. The 15 -> 36 byte encoding converts "555" to "S7S7S7" where "5" is original raw data, "S" is white space and "7" is Base53 encoded original data.

# Character Set

The source code contains the following:
```console
char charset_1536[] = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz#";
char chars_airgap[] = " \t\r\n"; /* white space */
```
Base53 in this case consists of the latin alphabet A-Z and a-z (both upper and lower case) as well as the pound sign as an additional special character. On top of this the space character is used to insert additional space.

# Tiny Tutorial

```console
% echo -n "hello" | ./1536
     kpJJkPi     #AAAAAA     #AAAAAA%
% echo -n "hellohellohello" | ./1536
     kpJJkPi     kpJJkPi     kpJJkPi%
% echo -n "uncle Leo says Jerry, hello" | ./1536
     jRRBmAj     DhDeOXR     DhHwEcF     lOhKeZi     kAgMKos     #KAAFEF%
% echo -n "Jerry, hello" | ./1536
     lOhKeZi     kAgMKos     #KAAFEF%
% echo -n "jRRBmAjEQjBTuE#FAAAAQ" | ./1536 -d
uncle Leo, %
```
Please note that the "%" character on the left is a prompt and on Mac OS X it is appended at the end in case CR/LF is missing, so the trailing % above is not produced by 1536 but the terminal software.

