# na4 (base77 encoder/decoder tool with integrated CRC4 and SHA256 support)
na4 is a tool to encode and decode binaries to/from ASCII format. The tool encodes data with a Base77 character set and includes CRC4 for robustness as well as optional SHA256 validation. The encoded data tends to get smaller than Base64. And the checksums makes it more robust.

There is no Makefile, but with almost no dependencies building the tool is very simple:
```console
% gcc -Wall -o 1654 1654.c
```

Quick tutorial to encode and decode data:
```console
% echo -n hello | ./na4
_216Hb[0W}%
% echo -n "_216Hb[0W}" | ./na4 -d
hello%
% echo -n hello again | ./na4 | ./na4 -d
hello again%
```

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
sha256 sum not present in parsed data
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
```

If the secret "sex laxar " is shared by both by the encoder and the decoder butis not part of the transmitted encoded stream then a correct SHA256 sum indicates that the encoded data has not been tampered with.

The utility follows standard Unix philosophy where lack or error message means success. Also if the decoder is used with the "-s" option and the received data stream is lacking SHA256 information this is treated as an error condition.