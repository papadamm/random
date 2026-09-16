# 1654 (base16 and base54 encoder/decoder tool)
1654 is a tool to encode and decode binaries to/from ASCII format. The tool supports a few different formats, mainly centered around Base54 but also hexadecimal Base16, mostly for convenience.

There is no Makefile, but building the tool is very simple:
```console
% gcc -Wall -o 1654 1654.c
```

After building the test script may be invoked:
```console
% ./test.sh
decode-xX16 PASS
encode-X16 PASS
decode-xX16:encode-X16 PASS
encode-x16:decode-xX16 PASS
encode-X16:decode-xX16 PASS
encode-54s:decode-54s PASS
encode-54s:decode-54ds PASS
encode-54d:decode-54d PASS
encode-54d:decode-54ds PASS
%
```

# Base54:

The 1654 tool implements Base54 with a 54 letter character set which is a subset of ASCII. It does this together with a particular encoding format with two different modes of operation. The Base54 design means in practice using 5.6875 bits per character. This in turn allows for three extra bits when encoding 8 bits of input data and 1 extra bit when encoding 16 bits of input data.

Base54 single mode:
- Encodes one byte of binary input data into two Base54 letters
- Is equally efficient as Base16 but is using a different character set
- May be used to encode any multiple of 8-bit binary input data
- Supports chained CRC to improve robustness
- Allows for user-specific Out-Of-Band data

Base54 dual mode:
- Encodes two bytes of binary input data into three Base54 letters
- Is using the same character set as single mode
- By itself dual mode only supports encoding pairs of input bytes
- May be combined with single mode trailing byte support for odd lengths
- There is no CRC or OOB

Space requirements before and after encoding:
- The original input file may for instance be 100 bytes
- When encoded as Base54 single mode the ASCII output will be 200 characters
- When encoded as Base54 dual mode the ASCII output will be 150 characters

Implementation details:
```console
Single mode example with ASCII input "CODE"

Input data (hexadecimal):         43      4F      44      45
ASCII input                       C       O       D       E
Encoded as Base54 single mode:  51  54  4F  6C  4F  55  51  56
ASCII                           Q   T   O   l   O   U   Q   V

In single mode the 8 input bits are split into two output letters.
Above the ASCII input letter "C" is encoded into "Q" and "T".
The first encoded letter contains 3 bits as flags and binary data.
The second encoded letter contains the remaining 8-bit worth of binary data.

Dual mode example with ASCII input "CODE"

Input data (hexadecimal):      43 4F     44 45
ASCII input                    C  O      D  E
Encoded as Base54 dual mode: 52 5d 4c  52 7d 76
ASCII                        R  ]  L   R  }  v

In dual mode almost all the space is used to squeeze in 16 bits of data.
Only a single flag bit is used in the first character. This flag allows
the decoder to differentiate between single and dual mode.
```

Details about the CRC in single mode:
```console
In single mode the data CRC is applied like a chain.
The first encoded byte includes flags. The second data only.
This gets repeated so every second byte contains data only.
The CRC covers the previous byte, the current one and the next.

CRC:           CC NN    PP CC NN

Encoded data:  SS TT UU VV WW XX YY ZZ

CRC:              PP CC NN    PP CC NN

PP = Previous (Omitted for the first byte)
CC = Current
NN = Next
```

Please note that if the goal is to enable hardening then using single mode only is preferred over mixed single and dual support. This to improve the accuracy of incorrect input data detection and not trigger dual mode detection.

Also please note that the CRC implementation will provide some very basic error detection at this point. It is recommended to consider using the OOB method and additional checksums for chunks of data to handle even more serious error conditions like for instance reordered or incorrectly repeated data. 

# Base16:

Examples with upper and lower case hexadecimal encoding and decoding:
```console
% echo -n "hello" | ./1654 encode-x16      
68656c6c6f%
% echo -n "hello" | ./1654 encode-X16
68656C6C6F%
% echo -n "68656c6c6f68656C6C6F" | ./1654 decode-xX16
hellohello%
```

It is obviously also possible to decode data from xxd minus white space:
```console
% echo -n "hello" | xxd -p | tr -d "[:space:]" | ./1654 decode-xX16
hello%
```
