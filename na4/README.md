# na4 (base77 encoder/decoder tool with checksum support)

na4 is a tool to encode and decode binaries to/from ASCII format. The tool encodes data with a Base77 character set and includes CRC4 for robustness as well as optional SHA256 validation. The encoded data tends to get smaller than Base64. And the checksums makes it more robust.


# Building 

There is no Makefile, but with almost no dependencies building the tool is very simple:
```console
% gcc -Wall -o na4 na4.c
```


# Tutorial

Encode and decode data like this:
```console
% echo -n hello | ./na4
_216Hb[0W}%
% echo -n "_216Hb[0W}" | ./na4 -d
hello%
% echo -n hello again | ./na4 | ./na4 -d
hello again%
```


# Efficiency

Compare encoding efficiency of base77 with base64 like this:
```console
% seq 539 | wc -c
    2048
% seq 539 | uuencode -m - | wc -c 
    2792
% seq 539 | ./na4 | wc -c  
    2688
% seq 539 | ./na4 -s 0 | wc -c
warning: SHA256 signature mode enabled with zero secret (aka naive mode)
    2736
```


# SHA256

Using the SHA256 feature in "naive mode":
```console
% echo -n hello | ./na4 -s 0
warning: SHA256 signature mode enabled with zero secret (aka naive mode)
_216Hb[0W}^D1Cx.M5W7HHzebaBf)N[sxz]D0SjqD7B<HnD7]]<qMfLel0%
% echo -n hello | ./na4 -s 0 | ./na4 -d
warning: SHA256 signature mode enabled with zero secret (aka naive mode)
hello%
% echo -n hello | ./na4 -s 0 2> /dev/null | ./na4 -d
hello%
% echo -n hello | ./na4 -s 0 2> /dev/null | ./na4 -d -s 0
warning: SHA256 signature mode enabled with zero secret (aka naive mode)
hello%
% echo -n hello | ./na4 -s 0 2> /dev/null| ./na4 -d -s 0 2> /dev/null
hello%
% echo -n hello | ./na4 | ./na4 -d -s 0
error: sha256 sum not present in parsed data
hello%
```
Please note that in "naive mode" (without a secret) it is possible that the encoded data stream may have been tampered with and there is no way for software to detect this. So without a secret the SHA256 signature shall not be trusted.

Also the above examples sometime include "2> /dev/stderr" which is used to redirect standard error to get rid of the warning message.

Using SHA256 with a secret prefix on stdin used as a suffix MAC:
```console
% echo -n "sex laxar i en laxask" | ./na4 -s 10
_8DaPIp)oMW_jej)A^D0TgqEwnVPNysE5iC5O9Qs<]D0<^0mYGzbigc*b4z@--gs3%
% echo -n "sex laxar _8DaPIp)oMW_jej)A^D0TgqEwnVPNysE5iC5O9Qs<]D0<^0mYGzbigc*b4z@--gs3" | ./na4 -d -s 10 
i en laxask%
% echo $?
0
% echo -n "sju laxar _8DaPIp)oMW_jej)A^D0TgqEwnVPNysE5iC5O9Qs<]D0<^0mYGzbigc*b4z@--gs3" | ./na4 -d -s 10 
error: sha256 sum mismatch
i en laxask%
% echo $?
1
```

Above the secret "sex laxar " is shared by the encoder and the decoder. When the secret is not included in the data stream and is kept private then a correct SHA256 sum indicates that the encoded data has not been tampered with. When for instance "sju laxar " is used as secret then the sha256 calculation will as indicate mismatch and the exit value is set accordingly.


# Note

Please note that the encoded data is not encrypted. Also in the case that the SHA256 sum calculation fails the tool will still output the data.

The utility follows standard Unix philosophy where lack of error message means success. Also if the decoder is used with the "-s" option and the received data stream is lacking SHA256 information this is treated as an error condition.
