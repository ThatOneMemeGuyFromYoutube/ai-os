# A forward relative jump skips an embedded byte string before entry.
jmp program
message: db 0x41, 0x73, 0x74, 0x65, 0x72, 0x4F, 0x53
program: mov eax, 42
exit
