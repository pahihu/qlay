del/q qltools.exe *.obj
rem bcc32x -o qltools.exe -I. -D__NT__=1 -O2 qltools.c nt\nt.c
gcc -m32 -s -o qltools.exe -I. -D__NT__=1 -O2 qltools.c nt\nt.c