# Quoted ASCII data is emitted inline; jump over it before executing code.
jmp program
message: db "Hello, AsterOS!", 0
program: mov eax, 42
exit
