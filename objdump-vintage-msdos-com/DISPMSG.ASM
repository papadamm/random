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

