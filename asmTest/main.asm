_text SEGMENT

plus:
        add rsi, rdi
        mov rax, rsi
        ret

main:
        mov rdi, 3
        mov rsi, 4
        call plus
        ret

mainCRTStartup PROC

call main
ret

mainCRTSTartup ENDP
_text ENDS
END