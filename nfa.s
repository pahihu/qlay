*
*	QLAY - Sinclair QL emulator
*	Copyright Jan Venema 1998
*	NFA.s
*

*****************************
* nfa.s
* 970610	initial code
* 970612	add io
* 970615	sector write
* 970616	correct bytecount, NFA
* 970628  5	delete files
* 970629  6	read file
* 970701  7	remove cache
* 970715  8	rename NFA to WIN
* 970717  81	reduce memory usage
* 970718  82	2c(a0).b
* 970719  83	debug read sector ovf
* 970722  80	truncate/close
* 970820  82h	rename
* 980521  86h	header \n
* 980606  87d	win1_ .. win8_
* 980606  87e	cleanup source
* 980607  87f	BOOT from win1
* 980607  87g	BOOT from win1, else mdv1
* 980607  87h	BOOT, Minerva compatible
* 980619  88a   save a2 in SETDRVNR
* 980629  89a   PRT device
* 990129  90c   PAR


********************************
END_CMD	equ	$00
RD_CMD	equ	$81
WR_CMD	equ	$82
DEL_CMD	equ	$84
GD_CMD	equ	$88
TRC_CMD	equ	$90
REN_CMD	equ	$a0
DCF_CMD	equ	$c0
RST_CMD	equ	$ff

CNTL	equ	$18100	b
DRIVENR	equ	$18101	b
FILENUM	equ	$18102	w
SECTNUM	equ	$18104	w
BYTENUM equ	$18106	w	offset from sector start
BYTECNT equ	$18108	w
STAT	equ	$1810a	w

PARPORT	equ	$180c0	b
PARDATA	equ	$0
PARSTRB	equ	$1
PARBUSY	equ	$2

SERPORT	equ	$180e0	b
SERDATA	equ	$0
SERSTRB	equ	$1
SERBUSY	equ	$2

* temporary stuff
T_STMSW equ	$18180
T_STLSW	equ	$18182
T_ENMSW	equ	$18184
T_ENLSW	equ	$18186
T_CMD	equ	$18188

SECTOR	equ	$18200

WIN0	equ	$57494e30	'WIN0'
MDV0	equ	$4d445630	'MDV0'
MDV	equ	$4d445600	'MDV\0'
BOOT	equ	$424f4f54	'BOOT'
UPPER	equ	$5f5f5f5f	andi to upper case

* BOOT loader
BOOTLOAD
	dc.l	$4AFB0001
	dc.w	PFTABLE
	dc.w	INITCODE
	dc.w	36
	dc.b	'NFA File, PAR, SER, 090c Jan Venema'
	dc.b	10

PFTABLE
	dc.w    $0001		proc/func table
	dc.w    DEVUSE-*
	dc.b    $07
	dc.b    'WIN_USE'
	dc.w    $0000
	dc.w    $0000
	dc.w    $0000

* init code
INITCODE
	bsr	INIT_BOOT
	bsr.b   INIT_PHYS
	bsr	INIT_PAR
	rts

****************************************
* initialize physical layer

INIT_PHYS
* 0.8	save A0/A3 during init, see AUG p300
* 0.87h save d1/d2 for minerva ??
	movem.l	a0/a3/d1/d2,-(a7)

	moveq.l #$18,d0
	moveq.l #$62,d1
	moveq.l #$0,d2
	trap    #$1		MT.ALCHP
	tst.l   d0
	bne.b   init_om
	lea.l   $1c(a0),a3
	lea.l   FILEIO.w(pc),a2
	move.l  a2,(a3)+
	lea.l   FILEOPEN.w(pc),a2
	move.l  a2,(a3)+
	lea.l   FILECLOSE.w(pc),a2
	move.l  a2,(a3)+
	adda.w  #$c,a3
	lea.l   FORMAT.w(pc),a2
	move.l  a2,(a3)+
	move.l  #$24,(a3)+
	move.w  #$3,(a3)+
	move.l  #WIN0,(a3)+
	lea.l   $18(a0),a0
	moveq.l #$22,d0
	trap    #$1		MT.LDD

* format available WIN drives
	link	a4,#-$a
	moveq.l	#$9,d0
	lea.l	FMTNAME.w(pc),a0
	movea.l	a4,a1
cpnam
	move.b	(a0)+,(a1)+
	dbra	d0,cpnam
	moveq.l	#$1,d3
chkdrv
	bsr.b	DRV_AVAIL
	beq.b	nxdrv
	move.l	a4,a0
	moveq.l #$3,d0		IO.FORMT
	trap    #$2
nxdrv
	addq.b	#1,5(a4)
	addq.l	#1,d3
	cmp.l	#8,d3
	ble.b	chkdrv
	unlk	a4
init_om
	movem.l	(a7)+,a0/a3/d1/d2
	rts

FMTNAME
	dc.w    7
	dc.b    'WIN1_20 '

DRV_AVAIL
	move.b	d3,DRIVENR
	move.b	#DCF_CMD,CNTL	do it
	move.w	STAT,d0		1: avail
	rts

****************************************
* initialize BOOT device

INIT_BOOT
	movem.l a0/a3,-(a7)
	moveq.l #$2c,d1		std calls + 4 scratchpad
	moveq.l #$18,d0
	moveq.l #$0,d2
	trap    #$1		MT.ALCHP
	tst.l   d0
	bne.b   boot_om
	lea.l   $1c(a0),a3
	lea.l   boot_io.w(pc),a2
	move.l  a2,(a3)+
	lea.l   boot_opn.w(pc),a2
	move.l  a2,(a3)+
	lea.l   boot_cls.w(pc),a2
	move.l  a2,(a3)+
	move.l	#$0,(a3)	scratchpad @ 28 = 0
	lea.l   $18(a0),a0
	moveq.l #$20,d0
	trap    #$1		MT.LIOD
boot_om
	movem.l (a7)+,a0/a3
	rts

w1boot
	dc.w	9
	dc.b	'WIN1_BOOT '

boot_io
boot_cls
	moveq	#-19,d0		niy, we'll never get here anyway...
	rts

boot_opn
	cmpi.w	#4,(a0)
	bne.b	boot_err
	move.l  $2(a0),d0
	andi.l  #UPPER,d0
	cmp.l   #BOOT,d0
	bne.b   boot_err

* how many times yet?

*	bsr.b	boot_iod
*	cmpi.b	#1,(a0)
*	beq.b	boot_err
*	addq.b	#1,(a0)

	cmpi.b	#1,$28(a3)
	beq.b	boot_err
	addq.b	#1,$28(a3)

* 087h	save a0, necessary for minerva
	move.l	a0,-(a7)
	lea	w1boot.w(pc),a0	try to open
	moveq	#1,d0
	moveq	#-1,d1
	moveq	#0,d3
	trap	#2
	tst.l	d0
	beq.b	boot_ex
	move.l	(a7)+,a0
	bne.b	boot_nf

* win1_boot exists
boot_ex
	moveq	#2,d0		close temp channel
	trap	#2
	move.l	(a7)+,a0
	lea.l   $48(a6),a1	SV_DDLST
	movea.l (a1),a1		get first entry
boot_try
	move.l  $26(a1),d3
	cmpi.l  #WIN0,d3
	beq.b	boot_found
	movea.l	(a1),a1
	move.l	a1,d1
	bne.b	boot_try

boot_nf

boot_err
	moveq.l #-$7,d0		not found
	rts

boot_found
	move.l  #MDV,$26(a1)	put MDV in, fix with next open MDV1_BOOT
	bra.b	boot_nf		do not open yet

* find my scratchpad, in a0
boot_iod
	lea.l	boot_opn.w(pc),a1
	move.l	$44(a6),a0	SV_DRLST
boot_try2
	cmpa.l	$8(a0),a1
	beq.b	boot_fdr
	move.l	(a0),a0
	move.l	a0,d1
	bne.b	boot_try2
boot_fdr
	lea.l	$10(a0),a0
	rts

*****************************************
* directory input/output

FS_LOAD
	moveq.l #$3,d0
	move.l  $24(a0),d7
	lsl.w   #$7,d7
	lsr.l   #$7,d7
	moveq.l #-$40,d2
	add.l   d7,d2
	bra     ro_sio

FS_RENAME:
	bsr     READONLY
	bne.b   ren_end
	move.w  (a1)+,d4	strlen
	subq.w  #$5,d4		'dev#_'
	bls.b   ren_bn
* 082h
*	cmpi.w  #$29,d4		??should be $24
	cmpi.w  #$24,d4
	bhi.b   ren_bn
	move.l  #-$20202001,d0	uppercase
	and.l   (a1)+,d0
	sub.b   $14(a2),d0
	cmp.l   $3e(a3),d0	drive name
	bne.b   ren_bn
	cmpi.b  #$5f,(a1)+	'_'
	beq.b   ren_do
ren_bn
	moveq.l #-$c,d0		bad name
ren_end
	rts
ren_do
* 082h
	bsr	REN_NFA		d4=fnlen, a1=filename, a0=fsdef
	bne.b	ren_end

	lea.l   $68(a0),a5
	move.l  a1,d7		(d7)=first letter of name
	move.w  $1e(a0),d5	file nr
	clr.w   $1e(a0)
ren_try
	addq.w  #$1,$1e(a0)
	moveq.l #$47,d0		like fs.headr
	moveq.l #$40,d2
	lea.l   $58(a0),a1
	bsr.b   IFILEIO
	beq.b   ren_noerr
	cmpi.w  #-$a,d0		EOF
	beq.b   ren_del
	bra.b   ren_quit
ren_noerr
	move.w  d4,d3
	movea.l d7,a1
	bsr     STRCMP
	bne.b   ren_try
	moveq.l #-$8,d0		already exists
ren_quit
	move.w  d5,$1e(a0)
	rts
ren_del
	lea.l   $58(a0),a2
	moveq.l #$12,d0
ren_clr
	clr.w   -(a2)		clear 38 bytes filename+len
	dbf     d0,ren_clr
	movea.l a2,a1		a1=2c(a0)
	movea.l d7,a5		(d7)=new name
	move.w  d4,(a2)+	d4=len
ren_cpy
	move.b  (a5)+,(a2)+
	subq.w  #$1,d4		copy string
	bgt.b   ren_cpy
	move.w  d5,$1e(a0)
	moveq.l #$e,d1
	moveq.l #$26,d2
	move.b  $2c(a0),-(a7)
	bsr.b   int_del
	move.b  (a7)+,$2c(a0)
	rts

* internal delete
int_del
	moveq.l #-$1,d0

* internal i/o
IFILEIO
	movem.l d0/d2-d7/a4-a5,-(a7)
iio_try
	movem.l (a7),d0/d2
	moveq.l #$1,d3
	bsr.b   FILEIO
	addq.l  #$1,d0		-1, not complete, try again
	beq.b   iio_try
	subq.l  #$1,d0
	addq.l  #$4,a7		skip d0
	movem.l (a7)+,d2-d7/a4-a5
	rts

**************************
*  	FILE I/O
FILEIO
	move.b  $1d(a0),d6
	bsr     ID2NAME
	tst.l   d0
	blt     INT_FS_HEADS
	cmpi.b  #$40,d0
	bcs     ro_sioe
	cmpi.b  #$4b,d0
	bhi.b   unk_bp
	sub.b	#$40,d0
	add.w   d0,d0
* bad assembler, code must read:
*	dc.w    $303b
*	dc.w    $0006
*	dc.w    $4efb
*	dc.w    $0002
	move.w	*+8(pc,d0.w),d0
	jmp	*+4(pc,d0.w)

IOTAB
	dc.w    FS_CHECK-IOTAB  	i/o jump table
	dc.w    FS_FLUSH-IOTAB
	dc.w    FS_POSAB-IOTAB
	dc.w    FS_POSRE-IOTAB
	dc.w    FS_UNKNW-IOTAB
	dc.w    FS_MDINF-IOTAB
	dc.w    FS_HEADS-IOTAB
	dc.w    FS_HEADR-IOTAB
	dc.w    FS_LOAD-IOTAB
	dc.w    FS_SAVE-IOTAB
	dc.w    FS_RENAME-IOTAB
	dc.w    FS_TRUNCATE-IOTAB

FS_UNKNW
unk_bp
	moveq.l #-$f,d0		bad parameter
	rts

FS_CHECK
FS_FLUSH
	moveq.l #$0,d0
	rts

FS_POSAB
	moveq.l #$0,d0
	bra.b   pos_do

FS_POSRE
	moveq.l #$0,d0
	move.l  $20(a0),d2
pos_calc
	lsl.w   #$7,d2
	lsr.l   #$7,d2
	subi.l  #$40,d2
	add.l   d2,d1
	bvs.b   pos_fin

pos_do
	move.l  d1,d2
	bmi.b   pos_beg
	addi.l  #$40,d2
	bvs.b   pos_fin
	asl.l   #$6,d2
	bvs.b   pos_fin
	add.l   d2,d2
	lsr.w   #$7,d2
	cmp.l   $24(a0),d2
	ble.b   pos_wr

pos_fin
	moveq.l #$0,d1
	move.l  $24(a0),d2
	moveq.l #-$a,d0
	bra.b   pos_calc

pos_beg
	moveq.l #$40,d2
	moveq.l #$0,d1
pos_wr
	move.l  d2,$20(a0)
	rts

FS_MDINF
	move.l  #WIN0,d0
	add.b   $14(a2),d0
	move.l  d0,(a1)+
	move.l  #$5f202020,(a1)+
	move.w  #$2020,(a1)+
	move.l  (a4),d1
	moveq.l #$0,d0
	rts

FS_SAVE
	moveq.l #$7,d0
	bra.b   ro_sio

FS_HEADR
	moveq.l #$3,d5
	cmpi.w  #$40,d2
	bgt.b   err_or
	move.l  a1,-(a7)
	bsr.b   head_set
	movea.l (a7)+,a2
	subi.l  #$40,(a2)
	rts
err_or
	moveq.l #-$4,d0		out of range
	rts

* d2=position, d1=offset
INT_FS_HEADS
	moveq.l #$7,d5
	bra.b   head_set0

FS_HEADS
	moveq.l #$7,d5
	moveq.l #$e,d2		* std QDOS 14 bytes

head_set
	moveq.l #$0,d1

head_set0
	moveq.l #$0,d4
	move.w  $1e(a0),d4
	beq     unk_bp
	move.w  d4,-(a7)
	move.l  $24(a0),-(a7)
	move.l  $20(a0),-(a7)
	clr.w   $1e(a0)
	move.l  $4(a4),$24(a0)
	subq.w  #$1,d4
	lsl.l   #$6,d4
	add.l   d4,d1
	move.l  d2,-(a7)
	bsr     pos_do
	move.l  (a7)+,d2
	move.l  d5,d0
	moveq.l #$0,d1
	bsr.b   ro_sioe
	move.l  (a7)+,$20(a0)
	move.l  (a7)+,$24(a0)
	move.w  (a7)+,$1e(a0)
	tst.l   d0
	rts

* check if read only
READONLY
	move.b  $1c(a0),d3
	subq.b  #$1,d3		FS.ACCESS see IO.OPEN D3
	beq.b   err_ro		(shared)
	subq.b  #$3,d3
	beq.b   err_ro		(directory)
	cmp.b   d0,d0
	rts
err_ro
	moveq.l #-$14,d0	read only
sio_rts
	rts

*simple i/o
ro_sioe
	ext.l   d1		=0 ??
	ext.l   d2		buffer length	(a1=buffer base ptr)

ro_sio
	cmpi.b  #$7,d0
	bhi     unk_bp		error BP
	moveq.l #$0,d7
	tst.l   d3
	bge.b   sio_1
	sub.l   d1,d7
sio_1
	subq.b  #$4,d0
	beq     unk_bp
	blt.b   sio_fetch
	bsr.b   READONLY
	bne.b   sio_rts
	moveq.l #-$1,d3
	subq.b  #$2,d0
	beq     unk_bp
	blt.b   sio_4
	bgt.b   sio_2
sio_fetch
	moveq.l #$0,d3
	addq.b  #$4,d0
	beq.b   sio_4
	moveq.l #$a,d3
	subq.b  #$2,d0
	beq.b   sio_2
	blt.b   sio_3
	lsl.w   #$8,d3
sio_2
	add.l   a1,d7
	move.l  d7,-(a7)
	add.l   d2,d7
	bsr.b   SECTORIO
	move.l  a1,d1
	sub.l   (a7)+,d1
	rts
sio_3
	lsl.w   #$8,d3
sio_4
	move.l  d1,-(a7)
	lea.l   $3(a7),a1
	move.l  a1,d7
	addq.l  #$1,d7
	bsr.b   SECTORIO
	move.l  (a7)+,d1
	rts

*******************************************
*sector i/o
SECTORIO
	move.l  $1e(a0),d5
	move.l  $20(a0),d4
	swap	d5
	move.w	d5,FILENUM
	swap	d5
	move.w	d5,SECTNUM
	move.w	d4,BYTENUM
	move.l	a1,d0
	swap	d0
	move.w	d0,T_STMSW
	swap	d0
	move.w	d0,T_STLSW
	swap	d7
	move.w	d7,T_ENMSW
	swap	d7
	move.w	d7,T_ENLSW
	move.w	d3,T_CMD


	cmp.l   $24(a0),d4	eblock/ebyte
	blt.b   sect_find
	bgt.b   err_eof
	tst.b   d3		= eof, r/w?
	blt.b   sect_1		read: eof, write: append
err_eof
	moveq.l #-$a,d0		EOF
sect_rts
	rts
sect_1
	tst.w   d4		ebyte
	beq.b   sect_0
sect_find
	bsr     FSECTOR
	bra.b   sect_2

sect_0
	cmp.l   a1,d7
	bls     sect_ok
	bsr     GVACANT
	bne.b   sect_rts	drive full

sect_2
	tst.w   d3
	beq.w   sect_ok
	move.l  a5,d0		idx in sect.map
	sub.l   a4,d0		start of sect.map
	subq.l  #$8,d0		d0=sect.nr
	lsl.l   #$7,d0		d0=sect.offset 64*4L=512
	lea.l   $8(a4,d0.l),a5	a5=sect.start
	move.w  $2(a4),d0	# of sectors
	lsl.w   #$2,d0		*4
	adda.w  d0,a5		skip sect.map
	adda.w  d4,a5		d4 is offset into sector! ext.l needed??

* Access a sector:
* d3=access type: wr/rd/byte..
* d4=nblock/nbyte, 20(a0)
* d5=filenr/blocknr
* a1=start address for transfer
* d7=end address for transfer (shortend if d3=0x000a.w)
* a5=drive sector address

	tst.w   d3
	bgt.b   sect_rd

	bsr.w   W_S_NFA
sect_wl
	cmp.l   a1,d7		write sector
	bls.b   sect_wend
* 0.8
	swap	d5
	cmpi.w	#0,d5
	swap 	d5
	beq.b	sect_upd	directory update
	addq.l	#1,a1
	addq.l	#1,a5
	bra.b	sect_nupd

sect_upd
	move.b  (a1)+,(a5)+
sect_nupd
	addq.w  #$1,d4
	btst    #$9,d4
	beq.b   sect_wl		loop
	addq.w  #$1,d5
	addi.l  #$fe00,d4

sect_wend
	st.b    $2c(a0)		file changed indicator
	cmp.l   $24(a0),d4
	blt.b   sect_fin
	move.l  d4,$24(a0)
	bra.b   sect_fin

sect_rd
	bsr.w   R_S_NFA		A5 points to SECTOR! A5 not used after sector rd.
	moveq.l #$0,d0		read sector
sect_rl
	cmp.l   a1,d7
	bls.b   sect_fin
	cmp.l   $24(a0),d4	eof passed?
	bge.b   sect_err
	move.b  (a5)+,d0	get byte	(from WIN sector memory)
	move.b  d0,(a1)+	store it	(to the application)
	cmp.w   d0,d3
	bne.b   sect_3		==0x000a
	move.l  a1,d7		fake end now
sect_3
	addq.w  #$1,d4
	btst    #$9,d4
	beq.b   sect_rl		loop
	addq.w  #$1,d5
	addi.l  #$fe00,d4

sect_fin
	move.l  d4,$20(a0)	wrap up
	cmp.l   a1,d7
	bhi     SECTORIO
	cmpi.w  #$a,d3
	bne.b   sect_ok
	cmp.b   d0,d3
	beq.b   sect_ok
	moveq.l #-$5,d0		buffer full
	rts
sect_ok
	moveq.l #$0,d0
	rts
sect_err
	move.l  d4,$20(a0)
	bra     err_eof

* find sector
* entry:  d5.l=file.nr/block.nr $20(a0) channel
* return: a5=block found, d0=error return.
FSECTOR
* 0.81	don't update anything unless directory file (0)
	swap	d5
	cmpi.w	#0,d5
	swap 	d5
	bne.b	fsect_end	no directory update

	lea.l   $8(a4),a5	sect.map
	move.w  $2(a4),d1	# of sects
	subq.w  #$1,d1
	moveq.l #-$10,d0	bad medium
fsect_cmp
	cmp.l   (a5)+,d5
	dbeq    d1,fsect_cmp
	subq.l  #$4,a5
fsect_end
	rts

* get vacant sector
* entry:  d5.l=file.nr/block.nr $20(a0) channel
* return: a5=vacant sect.map, d5.l in sect.map, # free sects--, d0=error ret.
GVACANT
* 0.81	don't update anything unless directory file (0)
	swap	d5
	cmpi.w	#0,d5
	swap 	d5
	bne.b	vac_end		no directory update

	lea.l   $8(a4),a5	sect.map
	move.w  $2(a4),d1	# of sects
	subq.w  #$1,d1
	moveq.l #-$b,d0		drive full
vac_1
	cmpi.b  #$FD,(a5)	vacant?
	addq.l  #$4,a5
	dbeq    d1,vac_1
	bne.b   vac_2		full
	move.l  d5,-(a5)	put d5.l in sect.map
	subq.w  #$1,(a4)	# of free sectors--
vac_end
	moveq.l #$0,d0
vac_2
	rts

*************************
* OPEN
FILEOPEN
	move.l	$3e(a3),d0	undo BOOT stuff
	cmp.l	#MDV,d0		win_use mdv gives 'MDV0'
	bne.b	opn_start
	move.l	#WIN0,$3e(a3)	reset to WIN
opn_start
	move.b  $1d(a0),d6
	bsr     ID2NAME
	beq.b   err_nf

* temporarily use the new channel ID, to fill in the directory header
	lea.l   $58(a0),a5	space for header
	moveq.l #$40,d2
	move.l  d2,$20(a0)	nblock/nbyte = 40
	move.l  $4(a4),$24(a0)	eblock/ebyte = dir file length
	cmpi.b  #$4,$1c(a0)	access type
	beq     OPENDIR
	moveq.l #$0,d4
	moveq.l #$0,d5

* try next one
opn_try
	addq.w  #$1,d5
	bsr     IFSTRG		get 40 bytes from dir into header
	bne.b   opn_eof
	tst.l   $58(a0)		filelength
	beq.b   opn_1
	lea.l   $32(a0),a1	?
	move.w  (a1)+,d3
	lea.l   $68(a0),a5	filename
	bsr     STRCMP
	bne.b   opn_try
	bra.b   FOUNDFILE

opn_1
	tst.w   d4
	bne.b   opn_try
	move.w  d5,d4		remember 1st 0length file number??
	bra.b   opn_try

err_nf
	moveq.l #-$7,d0		not found
	rts

err_ae
	moveq.l #-$b,d0		already exists
	rts

opn_eof
	cmpi.l  #-$a,d0		end of file
	bne.b   opn_rts

*				name not found, create a new file
	move.b  $1c(a0),d0	access
	blt.b   opn_ok		delete
	subq.b  #$2,d0
	blt.b   err_nf
	moveq.l #$0,d6
	tst.w   (a4)		# of free sectors
	beq.b   err_ae
	tst.w   d4		? free entry
	beq.b   opn_2

* write to file nr d4 position in directory
opn_wrf
	move.w  d4,d5		use free spot
	lsl.l   #$6,d4		multiply by 64
	lsl.l   #$7,d4		len->block/byte
	lsr.w   #$7,d4
	move.l  d4,$20(a0)

opn_2
	lea.l   $58(a0),a5
	move.l  d2,(a5)+	$40
	clr.w   (a5)+
	clr.l   (a5)+
	clr.l   (a5)+
	moveq.l #$12,d0
	lea.l   $32(a0),a1	? name in 32(a0)
opn_cpy
	move.w  (a1)+,(a5)+
	dbf     d0,opn_cpy	copy length + all 36 bytes of name!! sic
	clr.l   (a5)+
	clr.l   (a5)+
	clr.l   (a5)+
	bsr.b   ISSTRG		put $40 bytes in directory
	move.l  $24(a0),$4(a4)	eblock/ebyte (dir)

* now put in the new channel ID values, previously used for directory update:
	clr.l   $20(a0)		nblock/nbyte (0)
	move.l  d6,$24(a0)	eblock/ebyte ($40)
	move.w  d5,$1e(a0)	file nr
	bsr.b   ISSTRG		put $40 bytes in file header
	bra.b   opn_rts

*found matching file
FOUNDFILE
	move.b  $1c(a0),d0	access
	blt.b   DELFILE
	cmpi.b  #$2,d0
	beq.b   opn_ae		new (exclusive)
	bgt.b   opn_ovw		new (overwrite)
	move.l  $58(a0),d1	old (exclusive/shared)
	lsl.l   #$7,d1		length -> block/byte
	lsr.w   #$7,d1
	move.w  d5,$1e(a0)	file nr
	move.l  d2,$20(a0)	nblock/nbyte ($40)
	move.l  d1,$24(a0)	eblock/ebyte (old length)
opn_ok
	moveq.l #$0,d0
opn_rts
	rts
opn_ae
	moveq.l #-$8,d0		already exists
	rts

OPENDIR
	clr.w   $32(a0)		directory open
	bra.b   opn_ok

opn_ovw
	move.w  d5,d4		new (overwrite)
	moveq.l #$40,d6
	bsr.b   opn_wrf		write header in dir.
	bra.b   trc_do

* delete file  file nr=d5
DELFILE
	bsr.w   D_F_NFA

	moveq.l #$0,d4		from fblock 0
	bsr.b   CLRFMAP		clear drive map entries
	lsl.l   #$6,d5		* 64
	lsl.l   #$7,d5		pos -> block/byte
	lsr.w   #$7,d5
	move.l  d5,$20(a0)	temp ID, for dir update
	moveq.l #$40,d0
	lea.l   $98(a0),a5	fill bw. till $58 with 0
del_1
	clr.l   -(a5)
	subq.w  #$4,d0
	bgt.b   del_1
	bra.b   ISSTRG		write to directory

* internal i/o: fetch string
IFSTRG
	moveq.l #$3,d0
	bra.b   iss_1

* internal i/o: send string
ISSTRG
	moveq.l #$7,d0
iss_1
	lea.l   $58(a0),a1
	bra     IFILEIO

* string compare a1,a5
STRCMP
	cmp.w   -$2(a5),d3	unequal length, quit
	bne.b   str_rts
	bra.b   str_1
str_0
	bsr.b   UPPCASE
	move.b  d1,d0
	bsr.b   UPPCASE
	cmp.b   d1,d0
str_1
	dbne    d3,str_0
str_rts
	rts			return eq. if ==

* upper case char (a1,d3) -> d1
UPPCASE
	exg     a1,a5
	move.b  $0(a5,d3.w),d1
	cmpi.b  #$61,d1		'a'
	blt.b   upp_rts
	cmpi.b  #$7a,d1		'z'
	bgt.b   upp_rts
	subi.b  #$20,d1
upp_rts
	rts


FS_TRUNCATE
	bsr     READONLY
	bne.b   str_rts

trc_do
	move.l  $20(a0),d4
	move.l  d4,$24(a0)
*080(f)
	bsr	TRC_NFA

	subq.l  #$1,d4
	swap.w  d4
	addq.w  #$1,d4
	move.w  $1e(a0),d5

* truncate file d5.w, from block d4.w till end
CLRFMAP

* 0.81	don't update anything unless directory file (0)
	cmpi.w	#0,d5
	bne.b	clrf_1		no directory update (082: fci included)

	lea.l   $8(a4),a5	file/block list
	move.w  $2(a4),d0	# of sectors
	subq.w  #$1,d0
clrf_lp
	cmp.w   (a5)+,d5	== file?
	bne.b   clrf_0
	cmp.w   (a5),d4		=> block?
	bhi.b   clrf_0
	move.b  #-$3,-$2(a5)	'FD' available
	addq.w  #$1,(a4)	# of free sectors
clrf_0
	addq.l  #$2,a5
	dbf     d0,clrf_lp
clrf_1
	st.b    $2c(a0)		file changed indicator
	moveq.l #$0,d0
	rts

******************************************
* CLOSE
FILECLOSE
	movem.l d4-d7/a0/a4/a5,-(a7)
	move.b  $1d(a0),d6
	bsr.b   ID2NAME
	subq.b  #$1,$22(a2)	number of open files on medium
	tst.b   $2c(a0)		file change indicator
	beq.b   fcl_unl		no?->jump

	bsr	TRC_NFA

	move.l  $24(a0),d0	eblock/byte
	lsl.w   #$7,d0		bb->pos
	lsr.l   #$7,d0
	lea.l   $58(a0),a1
	move.l  d0,(a1)		filelength
	moveq.l #$0,d1		@0
	moveq.l #$4,d2		4 bytes
	bsr     INT_FS_HEADS
	move.l  a0,-(a7)
	moveq.l #$13,d0		MT.RCLCK -> d1.l
	trap    #$1
	movea.l (a7)+,a0
	move.l  d1,(a1)		file update date
	moveq.l #$34,d1		@34
	moveq.l #$4,d2		4 bytes
	bsr     INT_FS_HEADS

* 	unlink file def block
fcl_unl
	lea.l   $18(a0),a0	next channel ptr
	lea.l   $140(a6),a1
	movea.w $d4.W,a2	UT.UNLNK
	jsr     (a2)
	movem.l (a7)+,d4-d7/a0/a4/a5

* 	remove file def block
	movea.w $c2.W,a2	MM.RECHP
	jmp     (a2)


* get drive, phys def block -> a2, drive mem block -> a4
ID2NAME
	ext.w   d6		drive id from channel def. block
	lsl.b   #$2,d6
	lea.l   $100(a6),a2
	movea.l $0(a2,d6.w),a2	phys.def.block
	move.b  $14(a2),d6	drive nr from phys. def. block
	lsl.b   #$2,d6
	move.l  $3e(a3,d6.w),d6	drive mem. block ptr
	movea.l d6,a4
	rts			return eq: drive not allocated

*****************************
* FORMAT
* on entry: a1: a1 ptr to medium name, a3: linkage block, d1: drive number
FORMAT
	movea.l a3,a5
	lea.l   $18(a5),a4	next device
	lea.l   $100(a6),a0	fs phys. block
	moveq.l #$f,d0

fmt_nxte
	move.l  (a0)+,d2
	beq.b   fmt_nxt
	movea.l d2,a2		fs chan. block
	cmpa.l  $10(a2),a4	== driver routine?
	bne.b   fmt_nxt
	cmp.b   $14(a2),d1	== drive number?
	bne.b   fmt_nxt
	tst.b   $22(a2)		== number of open files
	beq.b   fmt_do
	moveq.l #-$9,d0		in use
	rts
fmt_nxt
	dbf     d0,fmt_nxte	no check for 16 ??
fmt_do
	moveq.l #$0,d6
	move.b  d1,d6
	lsl.b   #$2,d6
	move.l  $3e(a5,d6.w),d0
	beq.b   fmt_creat

*080(f)
	moveq.l	#-14,d0		format failed
	rts

* reclaim memory from old disc
	clr.l   $3e(a5,d6.w)	clear pointer to data buf
	move.l  a1,-(a7)
	move.l  a5,-(a7)
	movea.l d0,a0
	moveq.l #$19,d0
	trap    #$1		MT.RECHP
	movea.l (a7)+,a5
	movea.l (a7)+,a1

* create new drive
fmt_creat
	moveq.l #$0,d1
	move.w  (a1)+,d1	name length
	subq.w  #$5,d1		chop 'DEV#_'
	ble.b   fmt_ok		-> 0/0 end OK

* convert ascii number to int
	lea.l   $5(a1),a0
	move.l  a0,d7
	add.l   d1,d7
	movea.l a7,a1
	subq.l  #$2,a7
	move.l  a6,-(a7)
	suba.l  a6,a6
	movea.w $102.W,a2	CN.DTOI
	jsr     (a2)
	movea.l (a7)+,a6	word or long? tricky.
	move.w  (a7)+,d1	NR of sectors
	cmp.l   a0,d7
	bne.b   err_bn
	tst.l   d0
	bne.b   err_bn
	move.w  d1,d7
	beq.b   fmt_ok		-> 0/0 OK

* allocate memory for disc
	mulu.w  #$204,d1	* sector+map entry
	addq.l  #$8,d1		* free.w/total.w/dirlength.l
	moveq.l #$18,d0		MT.ALCHP
	moveq.l #$0,d2
	trap    #1
	tst.l   d0
	bne.b   fmt_rts
	move.w  d7,d1
	move.l  a0,$3e(a5,d6.w)	pointer to drive buffer
	move.w  d7,d0
*L_61e:  subq.w  #$1,d0
	move.w  d0,(a0)+	free
	move.w  d7,(a0)+	total
	move.l  #$40,(a0)+	dirlen
	clr.l   (a0)+		dir file in drive map

fmt_1
	move.l  #$FD000000,(a0)+	vacant
	subq.w  #$1,d0
	bgt.b   fmt_1

	bsr.b   get_dir

fmt_ok
	move.w  d1,d2		good=total
	moveq.l #$0,d0
fmt_rts
	rts
err_bn
	moveq.l #-$c,d0		bad name
	rts

*************************
* copy the NFA directory file into drive buffer
get_dir

	lea.l	$3e(a5,d6.w),a4
	move.l	(a4),a4
	movea.l	a4,a1
	lea.l	8(a4),a5	map ptr
	lsr.b   #$2,d6		drive nr
	moveq.l	#0,d1
	move.w	$2(a4),d1	# of sectors
	lsl.l	#2,d1
	addq.l	#8,d1
	adda.l	d1,a1		pointer to current sector buffer
	moveq.l #0,d2		current sector
	moveq.l	#0,d3		dirlen

g_next
	bsr.w	GD_NFA
	cmpi.w	#0,d0
	ble.b	g_end

* got sector, update map, free, dirlen
	move.l	d2,(a5)+
	addq.l	#1,d2
	subq.w	#1,(a4)		1 less free
	ext.l	d0
	add.l	d0,d3		dirlen
	move.l	d3,d0
	lsl.l	#7,d0		len->block/byte
	lsr.w	#7,d0
	move.l	d0,4(a4)
	cmpi.w	#0,d0		is byte!=0? -> last sector
	beq.b	g_next

g_end
	cmpi.l	#0,d3		was nothing loaded?
	bne.b	g_notfi
	subq.w	#1,(a4)		init default first
	move.l	#$40,4(a4)
	move.l	#0,(a5)+

g_notfi
	move.w	2(a4),d1	return total sectors
	rts

*************************
* DEVUSE procedure

DEVUSE
	bsr     dev_get
	bne.b   dev_rts
	subq.w  #$3,$0(a6,a1.l)
	bne.b   dev_bp
	move.l  $2(a6,a1.l),d6

* 0.8 enters here
	andi.l  #$5f5f5f00,d6
	addi.b  #$30,d6
	moveq.l #$0,d0
	trap    #$1		MT.INF
	movea.l $48(a0),a0	SV.DDLST
	lea.l   FILEIO.w(pc),a2
dev_lp
	cmpa.l  $4(a0),a2
	beq.b   dev_fin
	movea.l (a0),a0
	move.l  a0,d1
	bne.b   dev_lp
dev_bp
	moveq.l #-$f,d0
dev_rts
	rts
dev_fin
	move.l  d6,$26(a0)
	rts

dev_get
	moveq.l #$f,d0
	and.b   $1(a6,a3.l),d0
	subq.b  #$1,d0
	bne.b   dvg_1
	move.l  a5,-(a7)
	lea.l   $8(a3),a5
	movea.w $116.W,a2	CA.GTSTR
	jsr     (a2)
	movea.l (a7)+,a5
	bne.b   dvg_rts
	moveq.l #$3,d1
	add.w   $0(a6,a1.l),d1
	bclr    #$0,d1
	add.l   d1,$58(a6)
	bra.b   dvg_ok
dvg_1
	moveq.l #-$f,d0
	moveq.l #$0,d1
	move.w  $2(a6,a3.l),d1
	bmi.b   dvg_rts
	lsl.l   #$3,d1
	add.l   $18(a6),d1
	moveq.l #$0,d6
	move.w  $2(a6,d1.l),d6
	add.l   $20(a6),d6
	moveq.l #$0,d1
	move.b  $0(a6,d6.l),d1
	addq.l  #$1,d1
	bclr    #$0,d1
	move.w  d1,d4
	addq.l  #$2,d1
	movea.w $11a.W,a2	BV.CHRIX
	jsr     (a2)
	movea.l $58(a6),a1
	add.w   d4,d6
dvg_3
	subq.l  #$1,a1
	move.b  $0(a6,d6.l),$0(a6,a1.l)
	subq.l  #$1,d6
	dbf     d4,dvg_3
	subq.l  #$1,a1
	clr.b   $0(a6,a1.l)
dvg_ok
	moveq.l #$0,d0
dvg_rts
	rts

********************************
* get directory from native file system
* d6=drive, d2=sectnum, a1=dest. addr.
GD_NFA
	movem.l	a2/d2,-(a7)
	move.b	d6,DRIVENR
	move.w  d2,SECTNUM
	move.b	#GD_CMD,CNTL	do it
	move.w	STAT,d0		stat: >0 length, <0 error
*082				0: last sector was exactly 512

	move.w	d0,d2
*082
	ble.b	GD_FAIL

	lea	SECTOR,a2
GD_NEXT
	move.b	(a2)+,(a1)+
	subq.w	#1,d2
	bne.b	GD_NEXT
	bra.b	GD_END

GD_FAIL
	moveq.l	#-1,d0

GD_END
	movem.l	(a7)+,a2/d2
	rts

********************************
W_S_NFA
	movem.l	a1/a2/d0/d4,-(a7)
	bsr.w	SETDRVNR
	lea	SECTOR,a2
	moveq	#0,d0
	move.w	d4,d0
	adda.l	d0,a2
	move.w	d0,BYTENUM
	moveq.l	#0,d0

W_NEXT	cmp.l   a1,d7		write sector
	bls.b   W_END
	move.b  (a1)+,(a2)+
	addq.w	#1,d0
	addq.w  #$1,d4
	btst    #$9,d4
	beq.b   W_NEXT
W_END
	move.w	d0,BYTECNT
	move.b	#WR_CMD,CNTL	do it
	movem.l	(a7)+,a1/a2/d0/d4
	rts

********************************
* read a sector from disk into 'SECTOR' buffer
* start is offset by d4.w
* in: d4/l=nblock/nbyte, a1.l=start addr, d7.l=end addr
* out: no error return, a5=start of read data
R_S_NFA
	movem.l	a1/a2/d0/d1/d4,-(a7)
	bsr.w	SETDRVNR
	lea	SECTOR,a2
	movea.l	a2,a5		COPY DIRECTLY FROM SECTOR!! (0.7)
	adda.w	d4,a5		offset inside sector, ext.l needed??

	moveq	#0,d0
	move.w	d4,d0
*083
*	adda.l	d0,a2

* !! should be bytecnt:?
	move.w	d4,BYTENUM	first byte in sector

	move.l	d7,d0		end
	sub.l	a1,d0		start

*083 check if last byte fits in sector buffer
	move.l	d0,d1
	ext.l	d4
	add.l	d4,d1

	cmpi.l	#$200,d1
	ble.b	R_SHORT
	move.w	#$200,d0
	sub.w	d4,d0		minus the actual start point

R_SHORT

	move.w	d0,BYTECNT
	move.b	#RD_CMD,CNTL	do it

	movem.l	(a7)+,a1/a2/d0/d1/d4
	rts

********************************
* delete a file
D_F_NFA
	bsr.b	SETDRVNR
	move.w	d5,FILENUM
	move.b	#DEL_CMD,CNTL
	rts

********************************
* truncate/close a file
* FSB in a0
TRC_NFA
	movem.l	d1,-(a7)
	bsr.b	SETDRVNR

	move.w	$1e(a0),d1
	move.w	d1,FILENUM
	move.w	$24(a0),d1
	move.w	d1,SECTNUM
	move.w	$26(a0),d1
	move.w	d1,BYTENUM

	move.b	#TRC_CMD,CNTL	no errors

	movem.l	(a7)+,d1
	rts

********************************
* rename a file
* in: d4=fnlen, a1=filename, a0=fsdef
* out: error code in d0 (tst.l)
REN_NFA
	movem.l	d1/d4/a1/a2,-(a7)
	bsr.b	SETDRVNR

	move.w	$1e(a0),FILENUM

	lea	SECTOR,a2
	move.w	d4,BYTECNT
	subq.w	#1,d4
ren_l
	move.b	(a1)+,(a2)+
	dbra	d4,ren_l

	move.b	#REN_CMD,CNTL	do it
	move.w	STAT,d0
	ext.l	d0

	movem.l	(a7)+,d1/d4/a1/a2
	rts

********************************
* set DRIVENR
* a0=FSB
* save all regs

SETDRVNR
	movem.l	d1/a2,-(a7)
	move.b	$1d(a0),d1
	ext.w   d1		drive id from channel def. block
	lsl.b   #$2,d1
	lea.l   $100(a6),a2
	movea.l $0(a2,d1.w),a2	phys.def.block
	move.b  $14(a2),d1	drive nr from phys. def. block
	move.b	d1,DRIVENR
	movem.l	(a7)+,d1/a2
	rts

*******************************
* Centronics Parallel Interface Driver
* after AUG p274

INIT_PAR
	movem.l	a0/a3,-(a7)
	moveq	#$18,d0
	moveq	#$36,d1
	moveq	#$0,d2
	trap	#1
	tst.l	d0
	bne.b	par_exom

	lea	$1c(a0),a3
	lea	par_io.w(pc),a2
	move.l	a2,(a3)+
	lea	par_open.w(pc),a2
	move.l	a2,(a3)+
	lea	par_close.w(pc),a2
	move.l	a2,(a3)+
	lea	par_bp.w(pc),a2
	move.l	a2,(a3)+
	lea	par_outb.w(pc),a2
	move.l	a2,(a3)+
	move.w	#$4e75,(a3)+

	lea	$18(a0),a0
	moveq	#$20,d0
	trap	#1
par_exom
	movem.l	(a7)+,a0/a3
	rts

par_open
	move.w	$122,a4
	jsr	(a4)
	bra.b	par_exit
	bra.b	par_exit
	bra.b	par_alchp
	dc.w	3
	dc.b	'PAR',0		name is PAR
	dc.w	0		no parameters
par_alchp
	moveq	#$18,d1
	move.w	$c0,a4
	jmp	(a4)
par_close
	move.w	$c2,a4
	jmp	(a4)
par_io
	cmp.b	#$7,d0
	bhi.b	par_bp

	pea	$24(a3)		!
	move.w	$ea,a4
	jmp	(a4)

par_outb
	lea	PARPORT,a3
	tst.b	PARBUSY(a3)
	bmi.b	par_nc
	move.b	d1,PARDATA(a3)
	sf	PARSTRB(a3)
	st	PARSTRB(a3)
	moveq	#$0,d0
par_exit
	rts
par_nc
	moveq	#-$1,d0
	rts
par_bp
	moveq	#-$f,d0
	rts
