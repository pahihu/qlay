

  QLAY    Sinclair QL emulator for Windows95, DOS and Linux


Version 0.90, 19990130

1. Copyrights

This program is freeware. You may do with it whatever you want for
personal use. Copyright Jan Venema.

Permission is granted to redistribute this program free of charge,
provided it is distributed in the full archive with unmodified contents.
No profit beyond the price of the media on which it is distributed is
made.

There are no warranties of any kind for this program. If you use this
program, you do so at your own risk. The author is not responsible for
any damages that might result from using this program.

The QL ROMs and PSION programs are (C) Amstrad Plc. Distribution and
use on a non-commercial basis is allowed.

2. Overview

QLAY exists in three versions: Windows95, MS-DOS and Linux. In a general
sense, they have the same features, options and use. Specific exceptions
are noted where applicable.

The idea behind QLAY is to fully support the Sinclair QL hardware.
All original I/O is emulated. There is no focus on adding emulation
for hardware extensions like floppy or hard drives, video cards etc.
One exception is made: Native File Access allows access to the files of
the applicable OS: Windows95, DOS or Linux. With these hardware components
emulated, all original QL/QDOS software can be executed on QLAY. Newer
OSes like SMS are thus supported as long as they run fine on the original
QL hardware.

QLAY emulates the hardware of a Sinclair QL with 128, 640k or upto
8 Mbyte of memory.
It works with all known boot ROMs and ToolkitII ROMs.
In this version the following is emulated:
- Microdrives as basic file I/O. Up to 8 can be used.
- NFA. Access to MS-DOS/Linux file system as WIN1_ till WIN8_.
- Various screen sizes are supported to display in QL mode 256 and
  mode 512. Emulates: blank screen and 2nd screen bank (Alt Tab).
- Keyboard via IPC.
- International keyboards emulated: US, UK, GE, FR, IT.
- Mouse via memory mapped I/O.
- Emulation speed control and delay.
- IPC Beep sound emulation.
- Large memory: up to 8 Mbyte.
- Full MC68000 emulation, including *all* exceptions and trace.
- Place ROM cartridge files anywhere in memory.


3. Using QLAY

3.1 Requirements

- 386 CPU
- 4MB of memory.
- 4MB of harddrive space for program and additional swap space
  depending on emulated memory size. Swap file is called C:\cwsdpmi.swp (DOS).
  With 8MB or more memory no harddrive swapping needed with default
  emulation memory size.
- supported operating systems:
  * MS-DOS version, any of: DOS, Windows3.x, Windows95, OS/2, DOSEMU (linux)
  * Linux version: Linux with svgalib.
  * Windows95 version: Microsoft Windows-95[TM].

QLAY emulates comfortably and faster than the original QL on a
486/DX2-66MHz system with 16Mbyte memory.

3.2 Installation and use

Unzip the QLAY???.ZIP archive into a suitable directory.

To start use:
	QLAY -rJS.ROM
	After initializations the familiar Sinclair logo should appear.
	Press F1 or F2 and off you go!

	QLAY -h
	Gives short help on other options. (Not on Windows95 version).

If you downloaded the PSION package:
	QLAY -1quill.mdv -rJS.ROM
	After initializations the QUILL text editor that came with the
	original QL appears. Similar for abacus, easel and archive.


See section 3.3 for more options.

Special key combinations:
<CTRL> <ALT> <SHIFT> <X>  End QL emulation. (Not on Windows95 version).
<CTRL> <ALT> <SHIFT> <L>  Refresh screen (when switching between Windows
			  or OS/2 and QLAY DOS box).
<CTRL> <ALT> <SHIFT> <R>  RESET, like pushbutton on right hand side of QL

The microdrive file format is based on what is created during a FORMAT
command. It support 254 sectors of 512 bytes: almost 128kbytes.
The native OS MDV files have to be exactly 174930 bytes long.

3.2.1 MS-DOS
QLAY can be used from within a DOS box in Windows3.x, Windows95, OS/2
or dosemu of Linux. Via a 'PIF' or 'Shortlink' it can be started directly
from the graphical interface. A sample 'dosemu.conf' file for Linux is
included.
In addition, QLAY can be run from DOS, when the PC is booted in DOS.

QLAY requires a DPMI server. Windows has that support built in.
Under DOS it requires the 'CWSDPMI.EXE' included in the zip archive.

3.2.2 Linux
QLAY must be started with 'root' privilege from a console window. 'svgalib'
must be installed.

3.2.3 Windows95
QLAYW is a true Windows95 implementation of QLAY. The QL emulation window
executes with a GUI on the Windows desktop.

Installation:
- Unzip the archive.
- Move 'rsxnt.dll' in your windows\system directory.
- Place 'qlayw.exe' in your qlay directory.

3.2.4 Window95 Configuration Dialog

Every time qlayw is started, a configuration dialog is shown. All options
of qlayw can be modified and/or saved.
The program can be started from the Windows95 desktop via a shortlink,
without any knowlegde about command line options or configuration files.

Push [OK] to start, using the changes made to the configuration.
Push [CANCEL] to start, ignoring any changes made in the dialog.
Push [SAVE] to save new configuration, and then start with new configuration.
All configuration information is stored in file 'qlay.rc'.


3.3 Command line options

All command line options can be placed in a configuration file: 'qlay.rc'.
Simply place each command line option on a separate line. QLAY will
use these settings at emulation start.
All program options are the same for Windows95, DOS and LINUX versions.

-d num
Various screen sizes are supported. Mode 1 gives best aspect ratio, but is
slower than other modes. Modes 6 and 7 emulate correct aspect ration via
a high resolution screen on high-performance PCs.

-o
Use the 'old' graphics driver. QLAY082 and up have new, faster drivers,
but not tested on different graphics cards. Old driver may be more stable.

-m num
Memory size can be chosen: 128k, 640k, or 1M up to 8 Mbyte.
Default is 640k. num=0: 128k, num=1: 1M, ..., num=8: 8M.
(QLAY082: option -s is not supported any longer)

-r file
Use 'file' as BOOTROM. Always loaded at address 0. As default QLAY will
use file 'JS.ROM'.

-l win1@directory1\qlay.dir
-l win2@directory7\qlay.dir
until
-l win8@directory8\qlay.dir

-l mdv1@directorya\file1.mdv
-l mdv2@directoryb\file2.mdv
until
-l mdv8@directoryh\file8.mdv

These will tell qlay which directory or file to use for emulation of a
WIN#_ or MDV#_ device. After the '@' any drive, directory and file can
be specified.
WIN: The file must be called 'qlay.dir'. It contains the ql directory.
     Use the QLAYT support program to include files in 'qlay.dir'.
     NFA.ROM (087 version) is needed to support WIN1_ till WIN8_.
MDV: The mdvfile must be in the specific QLAY MDV image file format.
     Use the QLAYT support program to convert files to MDV format.
     An 'R' directly after the '@' makes the mdvfile readonly.

Examples:
	QLAY -l mdv1@d:\qlay\mdv\quill.mdv -rJS.ROM
	All mdv1_ operations will be made to the native DOS file.

	QLAY -l win2@d:\qlay\win2\qlay.dir -rJS.ROM
	Files in directory d:\qlay\win2\ are accessed via WIN2_.

-c address@file
Expansion ROM files, like cartridges and I/O expansions, can be placed
flexibly in memory. For each ROM a separate '-c ...' is needed. The file
will be loaded, starting at specified (hex) address.
E.g.: toolkit and nfa: QLAY -r js.rom -c c000@toolkit.rom -c c0000@nfa.rom
ROMs are write protected. QDOS limititions apply to address range.
On a real QL the ROM cartridge port is mapped at c000.

-f speed
Emulation speed control. This controls after how many instructions the
QLAY internal 1msec 'tick' should occur. With this 'tick' QLAY emulates
all timing sensitive functions: 50Hz interrupt, mouse sensitivity, BEEP
sound generation, MDV and some internal housekeeping.
The default value is '-f 400'. This emulates the 50Hz interrupt rate
about right on a 486/DX2 66MHz machine. On faster machines a larger value
should be used. The effect is easily seen: watch the cursor flash rate
and compare that to a real QL.
The SuperBasic program 'ft_bas' helps finding the proper value for -f.
This option does not change the emulation speed, only the interrupt rate
is controlled.
NOTE: date and time are not affected. QLAY uses the PC's clock for those.

-w delay
Emulation delay. Makes QLAY emulate slower for e.g. a game that plays
too fast. The emulation gets slower with higher values of the -w
option. The default value is '-w 0' for undelayed emulation speed.

-M
Disable mouse emulation.

-A
Disable ALT key support (Windows95 only).

-L xx
xx = US, UK, GE, FR, or IT.
Emulates keyboard differences for the different languages. Supports
special character entry, for example accented letters, and symbols.
Not much tested yet.


(old style options, still supported, deprecated)
-1 mdvfile
-2 mdvfile
until
-8 mdvfile
Emulate a microdrive cartridge. Up to 8 are supported. The option number
refers to the drive number. An 'R' directly after the drive number makes
the mdvfile readonly. The mdvfile must be in the specific QLAY MDV image
file format. Use the QLAYT support program to convert files to MDV format.
Examples:

	QLAY -1quill.mdv -rJS.ROM
	All mdv1_ operations will be made to the native DOS file.

	QLAY -1Rquill.mdv -2emptydsk.mdv -rJS.ROM
	mdv1_ is write protected, mdv2_ can be written to.


3.4 NFA support

Native File Access is supported via a small ROMable piece of QL code in
file nfa.rom.

Start emulation with:
	QLAY -r js.rom -c c000@nfa.rom [options]

The boot screen shows that NFA is now available.
Type 'dir win1_' to find MS-DOS/Linux files available via NFA.
Option '-l win...' allows access to upto 8 WIN_, see above.

When using a toolkit ROM or another ROM:
	QLAY -r js.rom -c c000@toolkit.rom -c c0000@nfa.rom

For NFA to work properly a 'qlay.dir' file must be present. It must
contain a 64-byte QDOS file header for each MS-DOS/Linux file that needs
to be accessed in QLAY. The 'qlay.dir' file can be created and updated
with the 'qlayt' file conversion tool. See section 5.

The free/good sectors numbers refer to the directory file only. Per 8
files one sector is used.

In QLAY each WIN#_ only supports files in that same directory.
Upto 159 files. Error checking to be improved.

NFA supports one extra SuperBasic command: win_use.
To replace all FLP1_ or MDV1_ accesses with WIN1_ in existing programs:
'win_use flp', or 'win_use mdv'.
To get the WIN1_ device back : 'win_use'.


3.5 Mouse

QLAY can emulate a mouse for use with Pointer Environment. The mouse
driver must be present before emulation starts. If no mouse driver is
found mouse support is skipped. This should normally be no issue at all
when starting from Windows or OS/2.
The mouse is supported via memory mapped I/O.

Win95: press F12 to suppress the double mouse pointer, press F12 again
to get the Windows mouse pointer back.

4.1 Development

QLAY was developed using DJGPP, RSXNTDJ, Cygwin and Linux. All sources
available starting with release 0.90. Other OSs should be easily to
port to.

Not supported yet: NET_,FLP_.
Minimal tested support for: SER1_,SER2_ and PAR_.

You're encoraged to enhance QLAY and improve compatibility with the
original QL hardware.

4.2 QLAY, how does it work?

QLAY consists of 3 parts: 68000 emulation, QL hardware emulation and
interfaces to the native operation system (Windows95, DOS or Linux).

After initialization and command line option evaluation the 68000
emulation starts. It will read one instruction, decode and execute it.
Part of execution can be a read or write access to memory. If it is
an access to the QL I/O area starting at 0x18000, QLAY will execute
the necessary QL hardware emulation directly. E.g.: read a bit from
the IPC (8049). If that implies e.g: reading a keyrow, QLAY will execute
a keyboard request in the Windows95/DOS/Linux interfaces part.

After each executed 68000 instruction, QLAY will check whether there
is internal housekeeping to be done. This is when exceptions, interrupts,
and timing events like the 1msec tick are handled.
QLAY will then proceed with the next instruction according current value
in program counter.

QLAY does not patch any ROMs and does not access any of the QDOS system
variables directly. This way any program, PROM or even a complete
QDOS replacement (SMS?) can be executed by QLAY. As long as it assumes
the hardware, I/O and memory map of the original Sinclair QL.

4.3 QLAY, how fast is it?

Emulation speed depends on many factors. CPU performance has the biggest
impact, but also what QDOS needs: memory access to the emulated screen
is slower than normal memory access. Short loops in the 68000 code are
executed much faster than large code segments; this due to the level 1
and level 2 caches that the CPU of the PC has.

A simple SuperBasic benchmark (qsbb_bas) shows how many print, function
and string manipulation loops can be executed within 20 seconds.
Higher number is faster.

Native 68k
System/CPU	print  function	string	Configuration/Emulator
QL original	  980	  840	 1100	128k, JS-ROM
SandyQL		 1080	 1040	 1360	512k, Floppy, Par. Port
GoldCard	 2580	 4280	 5680
Super GoldCard	 5680	10260	13820
Amiga 68040/25	 9480	16540	23000	AmigaQDOS (JS/3.24)
QXL-20		14300	27900	70640
Amiga 68060/50	10480	48750	73880	AmigaQdos 3.24 Beta (Blitter)

Emulated 68k
System/CPU	print  function	string	Configuration/Emulator
486/DX2-66	  660	  980	 1300	QLAY081 -d2 (DOS)
486/DX2-66	 1080	 1360	 1820	QLAY082	-d2 (DOS)
486/DX2-66	  920	 1440	 1840	QLAY083 -d2 (Linux)
Amiga A1200	 2500	 1800	 2000	QL emulator, with 68060/50MHz
Pentium/150	 2360	 3680	 4960	QLAY082
PentiumPro/200	 6000	 8000	11000	QLAY082 -f2900
Pentium/200	 5260	 9340	12660	QLAYW087 size 1, 16k colors
Pentium/150	 7120	10840	28120	QPC

5. Tools and manual

QLAY is supported by the QLAYT and QL-TOOLS-Q tools for file conversion.

5.1 QLAYT

QLAYT provides a set of tools and general support functions for the QL
emulator QLAY. The tools are needed because the QL and it's operating
system QDOS have some pecularities that do not match well with most
other Operating Systems. Most obvious difference is that QDOS files consist
of two parts: a header block and the actual data file. To allow transparent
access from the emulator program to the native file system it's needed
that the file header is stored separately.

The QLAYT program takes care of inserting or extracting files into/from
a directory file. It knows about datasize fields. And will import and
export files from other QDOS compatible systems and QLAY's MDV file format.

Read more about QLAYT in the manual file 'qlayt.man' in 'qlayt.zip'.


5.2 QL-TOOLS-Q

From other sources 'qltools' is available to convert files to/from floppy
format. 'qltoolsq' is a modified version of 'qltools' that allows to
copy all files from a QL floppy in a PC directory. The files can then
be used in QLAY directly.

Read more about QLTOOLS in the manual files in the 'qltoolsq.zip'.


5.3 QDOS introduction

An introduction in QDOS and SuperBasic can be found in file 'qdos.man'.
This file was take from the AMIGA QDOS emulator. It may be incomplete and
contains errors. Has anyone better information?


6. Credits

The QLAY M68000 emulation code is derived from UAE, an AMIGA emulator
done by Bernd Schmidt and many others.
QLTOOLSQ is derived from qltools, done by Giuseppe Zanetti and many others.


7. Revisions

7.1 Bugs

If QLAY fails, crashes or worse, there may be a hint in the 'qlay.log' file.

Some known bugs, or not yet implemented:
- win3_ .. win8_, mdv3_ .. mdv8_ not via Windows95 configuration dialog.
- Linux version not much tested yet.
- Linux qlay -d2 and -d3 is the same: 640x480.
- FLASH (mode 8) is not supported.
- MDV access may give 'bad medium'. Experiment with '-f' option.
- In some screen resolutions (-d) the power, mdv and win LED are invisible.
- Serial and parallel ports, unstable/untested (0.90)

Undocumented/untested/unstable development options:
-F100	: fake F1 during boot
-D	: 68k debug mode
Key combinations Control/Alt/Shift:
C/A/S B	: no screen refresh toggle
C/A/S D	: dump memory to file 'qlay.dmp'
C/A/S R : RESET

7.2 History

090	Win95: F12 suppresses double mouse pointer
	Free source code available
	Italian keyboard: Alt Gr+ supports # and @.
089	Never released
088	International keyboard emulation: US, UK, GE, FR and IT.
087	Support WIN1_ till WIN8_ (via NFA.ROM 087)
	WIN1_BOOT file recognized at start (via NFA.ROM 087)
	Windows95 configuration setup dialog
	RESET QL (C/A/S R or via menu in Windows95)
	ALT key support for Windows95, optional
086	More Windows95 improvements
	Basic mouse support in Windows95.
	Dump all messages and errors to 'qlay.log'
	Support SDATE/ADATE, repaired timezone bugs, Windows95 memory leaks
	Support FLP_ access via qltoolsq 2.7q external program
085	Windows95 and Dos updates
	Fixed emulation bug that made Archive 'edit' hang
	Windows95: support any screen size and display mode
084	Windows95 alpha version
083	Linux version of QLAY and QLAYT
	Linux: -d6 speed improvement
	Linux: 2 new options in QLAYT
082	Improved 68k emulation: exceptions, speed
	New grapics code
	Rename added in NFA
	More emulation memory, ROM file support
	New tool: QLAYT to support all file conversions
	Command line options in configuration file
	Remove clock bug: QLAY now uses the local PC time of day
081	Repaired QL mode 4 on 16-color screens
	Improved error messaging in 'qlay.log'
080	NFA
	Mouse
	BEEP
	New key combinations for 'exit' and 'refresh'
	F6-F10 emulate shift F1-F5, PgUp/Dn: alt enter, End: ctl space
	Improved interrupt emulation
	Emulation speed control
	Better IPC emulation
077	Dual screen support
	ROM write protection
	New screen code. Mode 4 is wrong on 16 color screens now
	Minerva ROM support
	Scroll Lock key
076	Improved STOP instruction
075	Bug fixes to support tools
	More screen modes on QLAY
070	First release

7.3 Current versions

qlayw     0.90c 19990130	Windows95
qlay      0.90c 19990130	MS-DOS
nfa.rom   0.90c 19990130	QLAY
qlayt     0.90c	19990130	MS-DOS
qltoolsq  2.7q	19980519	MS-DOS
qlay      0.83a	19970904	Linux
qlayt     0.83a	19970904	Linux

8. Info and updates

http://www.inter.nl.net/hcc/A.Jaw.Venema
A.Jaw.Venema@net.hcc.nl

Jan Venema
