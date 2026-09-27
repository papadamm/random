# na4 (base77 encoder/decoder tool with checksum and crypto support)

na4 is a tool to encode and decode binaries to/from ASCII format. The tool encodes data with a Base77 character set and includes CRC4 for robustness as well as optional HMAC-SHA256 signature validation. There is also optional AES-256-CTR encryption support.


# Build

There is no Makefile, but with almost no dependencies building the tool is very simple:
```console
% gcc -Wall -o na4 na4.c
```


# Help

```console
% ./na4 --help          
Usage: ./na4 [OPTIONS]

A portable, dependency-free UNIX streaming filter for framing,
authenticating, and encrypting data over standard pipes.

Operational Modes:
  -d              Decode mode (default: encode)
  -e              Enable AES-256-CTR encryption (requires -s)
  -s <len>        Authenticate stream using HMAC-SHA256 with a secret
                  prefix of <len> bytes read from the beginning of stdin

Informational:
  -h, --help      Display this help message and exit

Secret Delivery (Idiom):
  Secrets are not passed as CLI flags. Prepend the exact secret bytes to
  the beginning of stdin using 'echo' and specify its length via -s <len>:

  Encode & Encrypt:
    ( echo -n "passphrase"; cat payload.bin - ) | ./na4 -e -s 10 > secret.na4

  Decode & Decrypt:
    ( echo -n "passphrase"; cat secret.na4 - ) | ./na4 -d -e -s 10 > out.bin

Security & Framing Notes:
  * In encrypted mode (-e -s), a random salt and check token (Frame 0)
    are derived via a 100k-iteration PBKDF2 loop before streaming starts.
  * Stream authenticity is verified via HMAC-SHA256 in the tail frames.
  * To improve privacy the encoded data size varies in encrypted mode.
  * Zero-byte input produces zero-byte output in plain mode.
  * Signing zero-byte input requires -e to avoid dictionary vulnerabilities.

Exit Status:
  0   Success / Verified stream
  1   Authentication failure, incorrect secret, or corrupted framing
% 
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
warning: using potentially unsafe 0-byte secret
    2736
% ( echo -n X; seq 539 ) | wc -c 
    2049
% ( echo -n X; seq 539 ) | ./na4 -s 1 | wc -c
    2736
% ( echo -n X; seq 539 ) | ./na4 -s 1 -e | wc -c
    2765
```
As can be seen above comparing uudecode and the last example with encryption enabled, na4 with both SHA256 and AES-CTR support enabled is using less space than regular Base64.


# Signature

Using the HMAC-SHA256 checksum feature in "naive mode":
```console
% echo -n hello | ./na4 -s 0
warning: using potentially unsafe 0-byte secret
_216Hb[0W}^D1Cx.M5W7HHzebaBf)N[sxz]D0SjqD7B<HnD7]]<qMfLel0%
% echo -n hello | ./na4 -s 0 | ./na4 -d
warning: using potentially unsafe 0-byte secret
hello%
% echo -n hello | ./na4 -s 0 2> /dev/null | ./na4 -d
hello%
% echo -n hello | ./na4 -s 0 2> /dev/null | ./na4 -d -s 0
warning: using potentially unsafe 0-byte secret
hello%
% echo -n hello | ./na4 -s 0 2> /dev/null| ./na4 -d -s 0 2> /dev/null
hello%
% echo -n hello | ./na4 | ./na4 -d -s 0
error: empty stream or missing signature
hello%
```
Please note that this "naive mode" (without a secret) is only intended for testing purposes. In fact, in this mode it is possible that the encoded data stream may have been tampered with and there is no way for software to detect this.

[Also the above examples includes a few "2> /dev/stderr" which is common shell syntax used to redirect standard error elsewhere. This to get rid of the warning messages.]


Using HMAC-SHA256 with a secret prefix on stdin:
```console
% echo -n "sex laxar i en laxask" | ./na4 -s 10
_8DaPIp)oMW_jej)A^D0TgqEwnVPNysE5iC5O9Qs<]D0<^0mYGzbigc*b4z@--gs3%
% echo -n "sex laxar _8DaPIp)oMW_jej)A^D0TgqEwnVPNysE5iC5O9Qs<]D0<^0mYGzbigc*b4z@--gs3" | ./na4 -d -s 10 
i en laxask%
% echo $?
0
% echo -n "sju laxar _8DaPIp)oMW_jej)A^D0TgqEwnVPNysE5iC5O9Qs<]D0<^0mYGzbigc*b4z@--gs3" | ./na4 -d -s 10 
error: signature mismatch
i en laxask%
% echo $?
1
```

Above the secret "sex laxar " is shared by the encoder and the decoder. When the secret is not included in the data stream (it is assumed to be kept private) then a correct SHA256 sum indicates that the encoded data has not been tampered with. When for instance "sju laxar " is used as secret then the SHA256 calculation will as indicate mismatch and the exit value is set accordingly. Please note that in plaintext mode with the "-s" option the data will be decoded and output on stdout regardless of the result of the SHA256 calculation.


# Crypto

The "-e" option together with "-s" enables AES-256-CTR encryption:
```console
% echo -n "Xhello" | ./na4 -s 1 -e
[H21Ub{m[JX5pMtLMR)@2WutiApsn_20a1XQgVp^D0Tt@.tip@p,G00fAacSnRC]D1F{KE_o6>d4n4Hku^eAr6Y
% echo -n "X[H21Ub{m[JX5pMtLMR)@2WutiApsn_20a1XQgVp^D0Tt@.tip@p,G00fAacSnRC]D1F{KE_o6>d4n4Hku^eAr6Y" | ./na4 -d -e -s 1
hello%
% echo $?
0
% echo -n "Y[H21Ub{m[JX5pMtLMR)@2WutiApsn_20a1XQgVp^D0Tt@.tip@p,G00fAacSnRC]D1F{KE_o6>d4n4Hku^eAr6Y" | ./na4 -d -e -s 1
error: secret mismatch
% echo $?
1
```


# Note

The utility follows standard Unix philosophy where lack of error message means success. As usual the exit value may be used to check if the operation was successful or not.
