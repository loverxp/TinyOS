section .rodata
global embedded_filetest_start
global embedded_filetest_end

embedded_filetest_start:
    incbin "build/user/filetest.elf"
embedded_filetest_end:
