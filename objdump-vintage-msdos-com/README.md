Use objdump from GNU binutils to disassemble vintage MSDOS COM files

Check the date of the assembly program:
```console
% date -r DISPMSG.ASM
Mon Jan 23 04:32:22 CET 1995
```

Build a more modern binutils from source:
```console
% tar zxvf binutils-2.46.0.tar.gz
% mkdir install
% cd binutils-2.46.0
% ./configure --target=i386-pc-msdos --prefix=`cd ../install && /bin/pwd`
% make
% make install
```

(On x86_64-apple-darwin23.6.0 it seems a AR tool issue exists and can be
 worked around by adding PATH="/usr/bin:$PATH" before ./configure and make)


Extract uuencoded COM file binary:
```console
% cat DISPMSG.UUE | uudecode -o /dev/stdout > DISPMSG.COM
```


Display the COM file as hexadecimal values and ASCII:
```console
% hexdump -C DISPMSG.COM
00000000  0e 1f ba 0d 01 b4 09 cd  21 b4 4c cd 21 68 65 6a  |........!.L.!hej|
00000010  73 61 6e 20 24                                    |san $|
00000015
```


Use objdump to disassemble the file as a binary with adjusted offset:
```console
% ./install/bin/i386-pc-msdos-objdump -b binary -m i8086 \
   -M i8086,data16,intel --adjust-vma=0x100 -D DISPMSG.COM

DISPMSG.COM:     file format binary


Disassembly of section .data:

00000100 <.data>:
 100:	0e                   	push   cs
 101:	1f                   	pop    ds
 102:	ba 0d 01             	mov    dx,0x10d
 105:	b4 09                	mov    ah,0x9
 107:	cd 21                	int    0x21
 109:	b4 4c                	mov    ah,0x4c
 10b:	cd 21                	int    0x21
 10d:	68 65 6a             	push   0x6a65
 110:	73 61                	jae    0x173
 112:	6e                   	outs   dx,BYTE PTR ds:[si]
 113:	20 24                	and    BYTE PTR [si],ah
```

Compare the disassembly above with the actual source code:
```console
% cat DISPMSG.ASM
.model tiny
.code
org 100h
start:
        push    cs
        pop     ds
        lea     dx,msg
        mov     ah,9
        int     21h

        mov     ah,4ch
        int     21h

msg     db      'hejsan $'

end start
```

Note that the disassembler is unaware that 0x10d should be treated as data.
