/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	System dependencies
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

/*
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
*/
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_UTIME_H
#include <utime.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif

#if 0  /* Maybe this should be something else... anything missing? */
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <errno.h>
#include <assert.h>

#ifdef HAVE_LINUX_JOYSTICK_H
#if BROKEN_JOYSTICK_H == 1
#include "joystick.h"
#else
#include <linux/joystick.h>
#endif
#endif

#ifdef __NeXT__
#define S_IRUSR S_IREAD
#define S_IWUSR S_IWRITE
#define S_IXUSR S_IEXEC
#define S_ISDIR(val) (S_IFDIR & val)
struct utimbuf
{
    time_t actime;
    time_t modtime;
};
#endif

#if defined(__SASC) && defined(AMIGA)
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

int mkdir(const char *);
int rmdir(const char *);

#define S_ISDIR(m)	(m & S_IFDIR)
#define S_IRUSR		~S_IREAD
#define S_IWUSR		~(S_IWRITE|S_IDELETE)
#define S_IXUSR		~S_IEXECUTE

typedef unsigned short        mode_t;

#define strcasecmp stricmp

#endif

#ifdef __DOS__
# ifdef HAVE_PC_H
#  include <pc.h>
# endif
# include <io.h>
#else
# undef O_BINARY
# define O_BINARY 0
#endif

/* If char has more then 8 bits, good night. */
typedef unsigned char U8;
typedef signed char S8;

#if SIZEOF_SHORT == 2
typedef unsigned short U16;
typedef short S16;
#elif SIZEOF_INT == 2
typedef unsigned int U16;
typedef int S16;
#else
#error No 2 byte type, you lose.
#endif

#if SIZEOF_INT == 4
typedef unsigned int U32;
typedef int S32;
#elif SIZEOF_S32 == 4
typedef unsigned long U32;
typedef long S32;
#else
#error No 4 byte type, you lose.
#endif
typedef U32 A32;

#if defined(__SASC) && defined(AMIGA)
#define chip rhubarb
#endif	/* __SASC */

