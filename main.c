/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	Main and cmdline parsing
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "qlmem.h"
#include "qldisk.h"
#include "exe68k.h"
#include "debug.h"
#include "os.h"
#include "keybuf.h"
#include "spc-os.h"
#include "qlvers.h"
#include "qlio.h"

int ram_size = -1;
int use_debugger = 0;
int opt_use_mouse = 1;
int opt_use_altkey = 1;
int opt_throttle = 0;
int produce_sound = 1;
int fake_joystick = 0;
T_KEYB opt_keyb = KEYB_US;
U32 qlay1msec=400; /*0.82c*/
int fakeF1=0;
int iatrace=0;
int screen_res = 2;
int opt_busy_wait = 0;
int opt_new_gfx = 1;

#define MAXCART	4
#define MAXWIN	8
#define MAXMDV	8
#define MAXSER	2

char	romfile[256] = "js.rom";
char	opt_cartfn[MAXCART][256] = {"", "", "", ""};	/* not a flexible solution */
U32	opt_cartaddr[MAXCART]  = {0, 0, 0, 0};
int	currcart=0;
char	mdvfn[MAXMDV][256] = {"", "", "", "", "", "", "", ""};
char	winfn[MAXWIN][256] = {"", "", "", "", "", "", "", ""};
char	sername[MAXSER][256] = {"", ""};
/* 089a: not used */
int	winpresent[8] = {1,0,0,0,0,0,0,0};	/* win1_ default in curr.dir */

#ifndef __DOS__
char prtname[256] = "lpr ";
#else
char prtname[256] = "LPT1:";
#endif

static void usage(void)
{
	printf("QLAY - Sinclair QL emulator  %s  (C) Jan Venema\n",qlayversion());
	printf("Summary of command-line options:\n");
	printf("  -d mode              : Select resolution with the mode parameter.\n");
	printf("  -f delay             : 50Hz interrupt delay factor\n");
	printf("  -h                   : Print help\n");
	printf("  -r file              : Use file as ROM image in stead of ql.rom\n");
	printf("  -c address@file      : Use ROM cartridge file at address (hex)\n");
	printf("  -l mdv#@file         : Load file as mdv#_        # = 1..8\n");
	printf("  -l win#@dir\\qlay.dir : Load directory as win#_   # = 1..8\n");
	printf("  -l ser#@device       : Use device (COM1..COM4) as ser#_   # = 1..2\n");
	printf("  -m num               : RAM size (default=640k, 0=128k, else 'num' Mbyte).\n");
	printf("  -w num               : Slow the emulation down. High 'num' makes slower.\n");
	printf("  -o                   : Use save graphics mode; slower.\n");
	printf("  -M                   : Disable mouse.\n");
	printf("  -L xx                : Use keyboard xx. xx=US(default),UK,GE,FR,IT.\n");
	printf("\n");
	printf("Valid resolutions: 1 (640x350x16); 2 (640x400x256); 3 (640x480x256);\n"
	   "                   4 (800x600x256); 5 (800x600x16);\n"
	   "                   6 (1024x768x256); 7 (1024x768x16);\n"
	   "QLAY may choose to ignore the color mode/resolution setting.\n");
}

// rsxntdj
#if defined(__QLWIN32__) && defined(__DJGPP__)
#define optarg _optarg
    extern char *optarg;
#endif

static void parse_cmdline(int argc, char **argv)
{
int c;

	while(((c = getopt(argc, argv,
		"Al:L:Df:gd:htxF:ac:SJm:M0:1:2:3:4:5:6:7:8:r:H:op:w:")) != EOF)) {

	switch(c) {
	case 'h':
		usage();
		exit(0);
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
		strncpy(mdvfn[c-'1'], optarg, 255);
		mdvfn[c-'1'][255] = 0;
		break;
	case 'l':
		if (strncmp(optarg,"win",3)==0) {
			c=optarg[3]-'1';
			if ((c>=0)&&(c<8)) {
				strncpy(winfn[c], &optarg[5], 255);
if(0)				{int l;
					l=strlen(winfn[c]);
					if (winfn[c][l-1]!='\\') {
						winfn[c][l]='\\';
						winfn[c][l+1]=0;
					}
				}
				winfn[c][255] = 0;
//089a				winpresent[c]=1;
			} else {
				epr("Option error: -l %s\n",optarg);
			}
		} else if (strncmp(optarg,"mdv",3)==0) {
			c=optarg[3]-'1';
			if ((c>=0)&&(c<8)) {
				strncpy(mdvfn[c], &optarg[5], 255);
				mdvfn[c][255] = 0;
			} else {
				epr("Option error: -l %s\n",optarg);
			}
		} else if (strncmp(optarg,"ser",3)==0) {
			c=optarg[3]-'1';
			if ((c>=0)&&(c<2)) {
				strncpy(sername[c], &optarg[5], 255);
				sername[c][255] = 0;
			} else {
				epr("Option error: -l %s\n",optarg);
			}
		} else {
			epr("Option error: -l %s\n",optarg);
		}
		break;
	case 'p':
		strncpy(prtname, optarg, 255);
		prtname[255] = 0;
		break;
	case 'r':
		strncpy(romfile, optarg, 255);
		romfile[255] = 0;
		break;
	case 'c':
		if (currcart>=MAXCART) {
			epr("Cartridge option error: too many. %d > %d\n",currcart+1,MAXCART);
		} else {
			char tmpstr[256];
			strncpy(tmpstr,optarg,255);
			c=sscanf(tmpstr,"%x@%s",&opt_cartaddr[currcart],opt_cartfn[currcart]);
			if (c!=2) {
				epr("Cartridge option error: %d, %s\n",c,optarg);
				opt_cartfn[currcart][0] = '\0';
			}
			opt_cartfn[currcart][255] = 0;
			currcart++;
		}
		break;
	case 'S':
		produce_sound = 0;
		break;
	case 'f':
		qlay1msec = atoi(optarg);
		break;
	case 'D':
		use_debugger = 1;
		break;
	case 'J':
		fake_joystick = 1;
		break;
	case 'M':
		opt_use_mouse = 0;
		break;
	case 'A':
		opt_use_altkey = 0;
		break;
	case 'm':
		ram_size=atoi(optarg);
		break;
	case 'F':
		fakeF1=atoi(optarg);
		break;
	case 'L':
	        strupr(optarg);
		if (!strcmp(optarg, "US")) opt_keyb = KEYB_US;
		else if (!strcmp(optarg, "UK")) opt_keyb = KEYB_UK;
		else if (!strcmp(optarg, "GE")) opt_keyb = KEYB_GE;
		else if (!strcmp(optarg, "FR")) opt_keyb = KEYB_FR;
		else if (!strcmp(optarg, "IT")) opt_keyb = KEYB_IT;
//		else if (!strcmp(optarg, "SE")) opt_keyb = KEYB_SE;
		break;
	case 'd':
		screen_res = atoi(optarg);
		if (!(screen_res >= 1 && screen_res <= 7)) {
			epr("Bad video mode selected. Using default.\n");
			screen_res = 2;
		}
		break;
	case 't':
		opt_throttle=1;
		break;
	case 'x':
	    	iatrace=1;
		break;
	case 'w':
		opt_busy_wait = atoi(optarg);
		break;
	case 'o':
		opt_new_gfx=0;
		break;
	default: epr("Invalid option: %c, exiting",c);
	    	exit(1);
	} /* end switch */
	} /* end while */
}

static void parse_cmdline_and_init_file(int argc, char **argv)
{
FILE *f;
char file[256], *home;
char *buffer,*tmpbuf, *token;
char smallbuf[256];
unsigned int bufsiz;
int result;
int n_args;
char **new_argv;
int new_argc;

	strcpy(file,"");

#if !defined(__DOS__) && !defined(__mac__)
	home = getenv("HOME");
	if (home != NULL && strlen(home) < 240) {
		strcpy(file, home);
		strcat(file, "/");
	}
#endif

#if defined(__unix) && !defined(__DOS__)
	strcat(file, ".qlayrc");
#else
	if (argc) {
		strcat(file, argv[1]);
		argc--;
	}
	else strcat(file, "qlay.rc");
#endif

	f = fopen(file,"rb");
	if (f == NULL) {
		parse_cmdline(argc, argv);
		return;
	}

	fseek(f, 0, SEEK_END);
	bufsiz = ftell(f);
	fseek(f, 0, SEEK_SET);

	buffer = (char *)malloc(bufsiz+1);
	buffer[bufsiz] = 0;
	if (fread(buffer, 1, bufsiz, f) < bufsiz) {
		epr("Error reading configuration file\n");
		fclose(f);
		parse_cmdline(argc, argv);
		return;
	}
	fclose(f);

#ifdef __DOS__
	{char *tmp;

		while ((tmp = strchr(buffer, 0x0d)))
			*tmp = ' ';
		while ((tmp = strchr(buffer, 0x0a)))
			*tmp = ' ';
		while (buffer[0] == ' ')
			strcpy(buffer, buffer+1);
		while ((strlen(buffer) > 0) && (buffer[strlen(buffer) - 1] == ' '))
			buffer[strlen(buffer) - 1] = '\0';
		while ((tmp = strstr(buffer, "  ")))
			strcpy(tmp, tmp+1);
	}
#endif

	tmpbuf = my_strdup (buffer);
	n_args = 0;
	if (strtok(tmpbuf, "\n ") != NULL) {
		do {
			n_args++;
		} while (strtok(NULL, "\n ") != NULL);
	}
	free (tmpbuf);

	new_argv = (char **)malloc ((1 + n_args + argc) * sizeof (char **));
	new_argv[0] = argv[0];
	new_argc = 1;

	token = strtok(buffer, "\n ");
	while (token != NULL) {
		new_argv[new_argc] = my_strdup (token);
		new_argc++;
		token = strtok(NULL, "\n ");
	}
	for (n_args = 1; n_args < argc; n_args++)
		new_argv[new_argc++] = argv[n_args];
	new_argv[new_argc] = NULL;
	parse_cmdline(new_argc, new_argv);
}

int ql_main1(int argc, char **argv)
{

	epr("Initialization...\n");
	parse_cmdline_and_init_file(argc, argv);
	return 0;
}

int ql_main2()
{
	if (produce_sound && !init_sound()) {
		epr("Sound driver unavailable: Sound output disabled\n");
		produce_sound = 0;
	}

	init_debug();
	init_joystick();
	init_keybuf();
	init_memory();
	init_qldisk();
	init_QL();
	init_m68k();

	if (!use_debugger) {
//		epr("Going graphics mode\n");
		if (!init_graphics()) {
			epr("Could not initialize graphics, exiting.\n");
			exit(1);
		}
	}
	MC68000_reset();
	epr("Initializations done\n");

	return 0;
}

int ql_exit()
{
	exit_qldisk();
	return 0;
}

#ifndef __QLWIN32__
int main(int argc, char **argv)
{
	open_log();
	ql_main1(argc,argv);
	ql_main2();
	debug();
	if (!use_debugger) graphics_leave();
	close_joystick();
	ql_exit();
	close_log();
	return 0;
}
#endif /* not __QLWIN32__ */

#if defined(__BORLANDC__) || defined(_MSC_VER)
#define PATH_MAX	_MAX_PATH
#endif

void write_options(void)
{
	FILE	*f;
	int	i;
	char	*kb;
	char 	*rcfile[PATH_MAX];

#define QLRCFILE "qlay.rc"

	if (argc) strcpy(rcfile,argv[1]);
	else strcpy(rcfile,QLRCFILE);

	f=fopen(rcfile,"w");	/* txt */
	if (f==NULL) {
		fpr("cannot open %s for writing\n",rcfile);
		return;
	}
	fprintf(f,"-r %s\n",romfile);
	i=0;
	while (strlen(opt_cartfn[i])) {
		fprintf(f,"-c %08x@%s\n",opt_cartaddr[i],opt_cartfn[i]);
		i++;
	}
	for (i=0;i<MAXWIN;i++) {
		if (strlen(winfn[i])) fprintf(f,"-l win%d@%s\n",i+1,winfn[i]);
	}
	for (i=0;i<MAXMDV;i++) {
		if (strlen(mdvfn[i])) fprintf(f,"-l mdv%d@%s\n",i+1,mdvfn[i]);
	}
	for (i=0;i<MAXSER;i++) {
		if (strlen(sername[i])) fprintf(f,"-l ser%d@%s\n",i+1,sername[i]);
	}
	if (strlen(prtname)) fprintf(f,"-p %s\n",prtname);
	if (screen_res!=2) fprintf(f,"-d %d\n",screen_res);
	kb="USUKGEFRITSE";
	if (opt_keyb!=0) fprintf(f,"-L %c%c\n",kb[2*opt_keyb],kb[2*opt_keyb+1]);
	fprintf(f,"-f %d\n",qlay1msec);
	if (opt_busy_wait) fprintf(f,"-w %d\n",opt_busy_wait);
	if (fakeF1) fprintf(f,"-F %d\n",fakeF1);
	if (ram_size!=-1) fprintf(f,"-m %d\n",ram_size);
	if (use_debugger) fprintf(f,"-D\n");
	if (!opt_use_mouse) fprintf(f,"-M\n");
	if (!opt_use_altkey) fprintf(f,"-A\n");
	if (opt_throttle) fprintf(f,"-t\n");
	if (iatrace) fprintf(f,"-x\n");
	if (!opt_new_gfx) fprintf(f,"-o\n");
	fclose(f);
	fpr("Wrote options in %s\n",rcfile);
}
