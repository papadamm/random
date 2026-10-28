# asm11100-xlate (vintage text processing script)

In case you happen to have the file ASM11100.ZIP from the "PicBook 2" floppy,
(It is a ZIP file dated June 6 1995 with the MSDOS software MPASM Version 1.11)
there is a file "usrguide.txt" which contains instruction information for
MCUs such as PIC16C5X, PIC16CXX and PIC17CXX. There seems to be no copyright
information, but it can be assumed that the files were created by Microchip.

As an example, if you want to run this script to look for COMF then try:
```console
% ./asm11100-xlate.sh -f ~/picbook2-floppy/ASM11100.ZIP-unpack/usrguide.txt -b 12 COMF
./asm11100-xlate.sh: using file ~/picbook2-floppy/ASM11100.ZIP-unpack/usrguide.txt
./asm11100-xlate.sh: using bitwidth 12
COMF 0010 01df ffff
```

Congratulations, you are now aware of the "Complement f" instruction format.

Alternatively, if you want to look for the 16-bit instruction MULWF then try:
```console
% ./asm11100-xlate.sh -f ~/picbook2-floppy/ASM11100.ZIP-unpack/usrguide.txt -b 16 MULWF  
./asm11100-xlate.sh: using file ~/picbook2-floppy/ASM11100.ZIP-unpack/usrguide.txt
./asm11100-xlate.sh: using bitwidth 16
./asm11100-xlate.sh: WARNING: unable to find MULWF
```
Unfortunately there seems to be no support for the MULWF instruction.

As a reference the file usrguide.txt comes with the following information:
```console
3. Connect worldwide to the Microchip BBS using the    
CompuServe communications network.  In most cases a local    
call is your only expense.  The Microchip BBS connection    
does not use CompuServe membership services, therefore you    
do not need CompuServe membership to use the Microchip BBS.    
    
The procedure to connect will vary slightly from country to    
country.  Please check with your local CompuServe agent for    
details if you have a problem.  CompuServe services allow    
multiple users at baud rates up to 9600.    
    
4. Contact the Microchip Technology Development    
Systems Information and Upgrade Line at 1-800-755-2345 in    
the United States and parts of Canada and 1-602-786-7320 
from everywhere else.    
    
5. Contact the Factory Applications Group.    
```

I feel slightly let down not having the color fax option, however the
9600 bps baudrate seems like a realistic conservative baud rate choice.