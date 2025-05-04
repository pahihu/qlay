del/q qxltool.exe
rem bcc32x -o qxltool.exe -INT -D__WINNT=1 -O2 qxltool.c NT/getopt.c NT/fnmatch.c
gcc -m32 -s -o qxltool.exe -INT -D__WINNT=1 -O2 qxltool.c NT/getopt.c NT/fnmatch.c