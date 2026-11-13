# 51kt313t001

This little slow performing utility may be used to configure the radio.

Display the current configuration:
```console
% ./51kt313t001 /dev/tty.usbserial-D30H6JW9      
ATI RFD SiK 2.0 on HM-TRP 
ATS1? 57
ATS2? 64
ATS3? 25
ATS4? 20
ATS5? 0
ATS6? 0
%
```

For instance, this turns off MAVLink optimization:
```console
% ./51kt313t001 /dev/tty.usbserial-D30H6JW9 ATS6=0
ATS6=0 OK
%
```

Save configuration in local storage:
```console
% ./51kt313t001 /dev/tty.usbserial-D30H6JW9 "AT&W"
AT&W OK
%
```

Compiles and executes on Mac OS X and/or Linux.