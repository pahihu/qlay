/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Make version and time
*/

#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
char out[100],in[100];
struct tm *tm;
int out_length = 100;
time_t when;
char *format;
FILE *f;

	if (argc != 2) {
		printf("Usage: makevers revision\n");
		exit(1);
	}
	if ((f=fopen(argv[1],"r")) == NULL) {
		printf("makevers: cannot open %s\n",argv[1]);
		exit(1);
	}
	fscanf(f,"%s",in);
	time(&when);
	tm = localtime(&when);
	format = "%a %b %d %H:%M:%S %Y";
	strftime(out, out_length, format, tm);
	printf("#include \"qlvers.h\"\n\n");
	printf("char *qlayversion(void)\n{\n");
	printf("\treturn \"%s %s\";\n}\n",in,out);
	printf("char *qlayversion2(void)\n{\n");
	printf("\treturn \"%s\";\n}\n",in);
	return 0;
}
