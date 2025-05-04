/*--------------------------------------------------------------.
| qxltool.h : Access QXL.WIN files from other operating systems |
|                                                               |
| (c) Jonathan Hudson, April 1998                               |
| No Warranty                                                   |
`--------------------------------------------------------------*/

#ifndef _QXLTOOL_H
#define _QXLTOOL_H

#include <stdint.h>

#if defined(__WINNT)
typedef unsigned char u_char;
typedef unsigned short u_short;
#else
#include <sys/time.h>
#include <unistd.h>
#endif
#include "version.h"

#define UNIX2QL    283996800
#define QL2UNIX   -(UNIX2QL)

#define VERSTR   VERSION ", " __DATE__

#if defined __GNUC__
#define PACKED  __attribute__ ((packed))
#else
#define PACKED
#endif

#define time32_t int32_t

#if defined(__APPLE__)
# define DEFQXL "/dos/QXL.WIN"
# define SHELL "SHELL"
# define TARGET "macOS"
#elif defined (__unix__)
# define DEFQXL "/dos/QXL.WIN"
# define SHELL "SHELL"
# define TARGET "Linux"
#elif defined (__WINNT)
# define DEFQXL "C:\\QXL.WIN"
# define SHELL "COMSPEC" 
# define TARGET "WIN32"
#else
# define DEFQXL "win2_tmp_qxl.win"
# define TARGET "QDOS"
# define SHELL  "SHELL"
#endif

#ifdef NEED_U_CHAR
 typedef unsigned char u_char;
#endif

#ifdef NEED_U_SHORT
 typedef unsigned short u_short;
#endif

#ifdef HAVE_READLINE_READLINE_H
# include <readline/readline.h>
#endif

#ifdef HAVE_READLINE_HISTORY_H
# include <readline/history.h>
#else
// Oh dear cygwin B20 defines readline.h but not history.h
extern void add_history(char *);
#endif

#define QLPATH_MAX 36

#if defined(_MSC_VER)
#pragma pack(push,1)
#endif

typedef struct PACKED
{
    char id[4];
    u_short namlen;
    u_char name[20];
    u_short dummy0;
    u_short randd;
    u_short access;
    u_short dummy1;
    u_short sectc;
    u_short dummy2[3];
    u_short total;
    u_short free;
    u_short formatted;
    u_short dummy3;
    u_short first;
    u_short direct;
    uint32_t dlen; 
    u_short dummy4[3];
    u_short map[1];
} HEADER;

typedef struct PACKED
{
    uint32_t length;
    u_short type;
    uint32_t data;
    uint32_t  dummy1;
    u_short nlen;
    u_char name[QLPATH_MAX];
    time32_t date;
    u_short dummy2;
    u_short map;
    uint32_t dummy3;
} QLDIR;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

typedef struct 
{
    union 
    {
        char xtcc[4];
        int32_t x;
    } x;
    uint32_t dlen;
} XTCC;

#if defined(_MSC_VER) || defined(__BORLANDC__)
#define PATH_MAX	_MAX_PATH
#endif

typedef struct
{
    int fd;
    HEADER h;
    QLDIR curd;
    QLDIR root;
    QLDIR tmpd;    
    QLDIR lastd;
    FILE *fp;
    short close;
    char fn[PATH_MAX];
    int mode;
    short fmode;
} QXL;

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifndef offsetof
# define offsetof(sname,fname) ((int32_t)&((sname *)0)->fname)
#endif

typedef long (*PCALLBACK)(QXL *, QLDIR *, void *, void *, u_short);
typedef int (*ACTFUNC)(QXL *, short , char **);

typedef struct
{
    char *name;
    ACTFUNC func;
    short flag;
} JTBL;

#define DO_RECURSE (1 << 0)
#define DO_BEST    (1 << 1)
#define DO_WILD    (1 << 2)

#define QX_TEXT    (1 << 0)
#define QX_OPEN    (1 << 1)
#define QX_FAIL_SILENTLY (1 << 14)
#define QX_ARGV0_IS_FP (1 << 15)
#endif
