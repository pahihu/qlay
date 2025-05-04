# qltools
# modifications for DJGPP 980518

cc = gcc

OBJS = qltools.o msdos.o

CFLAGS	= -I.
LDFLAGS =

qltools.exe : $(OBJS)
	$(CC) $(OBJS) -o qltools
	strip qltools
	coff2exe qltools

msdos.o : msdos/msdos.c qltools.h
	$(CC) -c $(CFLAGS) msdos/msdos.c

qltools.o : qltools.c qltools.h
	$(CC) -c $(CFLAGS) qltools.c

clean:
	-del *.o
	-del qltools
