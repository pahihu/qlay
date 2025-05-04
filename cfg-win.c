/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QLAY configuration dialog
*/

#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sysconfig.h"
#include "sysdeps.h"
#include "options.h"
#include "winmain.h"
#include "cfg-win.h"
#include "spc-os.h"

static HINSTANCE hInstance;

static void addstring(HWND hWnd, int box, char* str)
{
	SendDlgItemMessage(hWnd, box, CB_ADDSTRING, 0, (LPARAM) ((LPSTR) str));
}

static void init_options(HWND hWnd)
{
char tmpfield[256];
int opt;
int mso[]={1,0,2,3,-1,4,-1,-1,-1,5};

	/* default screen size */
	opt=screen_res;
	SendDlgItemMessage(hWnd, ID_CBX_SS, CB_RESETCONTENT, 0, 0);
	addstring(hWnd,ID_CBX_SS, "1: 512x256");
	addstring(hWnd,ID_CBX_SS, "2: 512x341");
	addstring(hWnd,ID_CBX_SS, "3: 768x512");
	addstring(hWnd,ID_CBX_SS, "4: 1024x683");
	addstring(hWnd,ID_CBX_SS, "5: 1024x768");
	SendDlgItemMessage(hWnd, ID_CBX_SS, CB_SETCURSEL, opt-1, 0);

	/* memory size */
	opt=ram_size; /* valid options -1, 0, 1, 2, 4, 8 */
	if (mso[opt+1]==-1) opt=-1;	/* default to 640k */
	SendDlgItemMessage(hWnd, ID_CBX_MS, CB_RESETCONTENT, 0, 0);
	addstring(hWnd,ID_CBX_MS, "128k");
	addstring(hWnd,ID_CBX_MS, "640k");
	addstring(hWnd,ID_CBX_MS, "1M");
	addstring(hWnd,ID_CBX_MS, "2M");
	addstring(hWnd,ID_CBX_MS, "4M");
	addstring(hWnd,ID_CBX_MS, "8M");
	SendDlgItemMessage(hWnd, ID_CBX_MS, CB_SETCURSEL, mso[opt+1], 0);

	/* default keyb language */
	opt=opt_keyb;
	SendDlgItemMessage(hWnd, ID_CBX_KBD, CB_RESETCONTENT, 0, 0);
	addstring(hWnd,ID_CBX_KBD, "US");
	addstring(hWnd,ID_CBX_KBD, "UK");
	addstring(hWnd,ID_CBX_KBD, "GE");
	addstring(hWnd,ID_CBX_KBD, "FR");
	addstring(hWnd,ID_CBX_KBD, "IT");
//	addstring(hWnd,ID_CBX_KBD, "SE");
	SendDlgItemMessage(hWnd, ID_CBX_KBD, CB_SETCURSEL, opt, 0);

	/* NFA addresses */
	opt=0;
	if(opt_cartaddr[0]==0x0c000)opt=0;
	if(opt_cartaddr[0]==0x10000)opt=1;
	if(opt_cartaddr[0]==0xc0000)opt=2;
	if(opt_cartaddr[0]==0xc4000)opt=3;
	if(opt_cartaddr[0]==0xc8000)opt=4;
	if(opt_cartaddr[0]==0xcc000)opt=5;
	SendDlgItemMessage(hWnd, ID_ADR_NFA, CB_RESETCONTENT, 0, 0);
	addstring(hWnd,ID_ADR_NFA, "0c000");
	addstring(hWnd,ID_ADR_NFA, "10000");
	addstring(hWnd,ID_ADR_NFA, "c0000");
	addstring(hWnd,ID_ADR_NFA, "c4000");
	addstring(hWnd,ID_ADR_NFA, "c8000");
	addstring(hWnd,ID_ADR_NFA, "cc000");
	SendDlgItemMessage(hWnd, ID_ADR_NFA, CB_SETCURSEL, opt, 0);

	/* other addresses */
	opt=1;
	if(opt_cartaddr[1]==0x0c000)opt=0;
	if(opt_cartaddr[1]==0x10000)opt=1;
	if(opt_cartaddr[1]==0xc0000)opt=2;
	if(opt_cartaddr[1]==0xc4000)opt=3;
	if(opt_cartaddr[1]==0xc8000)opt=4;
	if(opt_cartaddr[1]==0xcc000)opt=5;
	SendDlgItemMessage(hWnd, ID_ADR_OTHER, CB_RESETCONTENT, 0, 0);
	addstring(hWnd,ID_ADR_OTHER, "0c000");
	addstring(hWnd,ID_ADR_OTHER, "10000");
	addstring(hWnd,ID_ADR_OTHER, "c0000");
	addstring(hWnd,ID_ADR_OTHER, "c4000");
	addstring(hWnd,ID_ADR_OTHER, "c8000");
	addstring(hWnd,ID_ADR_OTHER, "cc000");
	SendDlgItemMessage(hWnd, ID_ADR_OTHER, CB_SETCURSEL, opt, 0);

	/* ROM files */
	SetDlgItemText(hWnd, ID_ROM_BOOT, romfile);
	SetDlgItemText(hWnd, ID_ROM_NFA, opt_cartfn[0]);
	SetDlgItemText(hWnd, ID_ROM_OTHER, opt_cartfn[1]);

	/* MDV files */
	SetDlgItemText(hWnd, ID_MDV1, mdvfn[0]);
	SetDlgItemText(hWnd, ID_MDV2, mdvfn[1]);

	/* WIN files */
	SetDlgItemText(hWnd, ID_WIN1, winfn[0]);
	SetDlgItemText(hWnd, ID_WIN2, winfn[1]);

	/* check boxes */
	opt=0;
	if (opt_use_mouse==0) opt=1;
	CheckDlgButton(hWnd, ID_CHK_MOUSE, opt);
	opt=0;
	if (opt_use_altkey==0) opt=1;
	CheckDlgButton(hWnd, ID_CHK_ALTKEY, opt);
	opt=0;
	if (opt_throttle==1) opt=1;
	CheckDlgButton(hWnd, ID_CHK_THROTTLE, opt);


	/* text edit boxes */
	SetDlgItemInt(hWnd, ID_SPEED, qlay1msec, FALSE);
	SetDlgItemInt(hWnd, ID_DELAY, opt_busy_wait, FALSE);
}

char szDirName[256];

static LPCTSTR gcd(char *fn)
{
char	*s;
LPCTSTR rv;

	strcpy(szDirName,fn);
	s = szDirName + strlen(szDirName);
	while (s > szDirName && *--s != '\\');
	*s = '\0';
	return (LPCTSTR) szDirName;
}

static BOOL CALLBACK Dialog(HWND hWnd, UINT wMsg, WPARAM wPar, LPARAM lPar)
{
static char szFile[256];
static char szDirName[256];
char tmpfield[256];
OPENFILENAME ofn;
int id, bRet, ms, ss;
int writefile,opt;

	writefile=0;
	switch (wMsg) {
        case WM_INITDIALOG:
		init_options(hWnd);
		return TRUE;

	case WM_COMMAND:
//		fpr("wcmd1 def, %d, %d, %ld\n",wMsg,wPar&0xffff,lPar);
		switch(wPar) {
		case ID_SAVE:
			writefile=1;
			/* fall through */

		case ID_OK:
			opt_use_altkey = 1 - IsDlgButtonChecked(hWnd, ID_CHK_ALTKEY);
			opt_use_mouse = 1 - IsDlgButtonChecked(hWnd, ID_CHK_MOUSE);
			opt_throttle = IsDlgButtonChecked(hWnd, ID_CHK_THROTTLE);

			GetDlgItemText(hWnd, ID_ROM_BOOT, romfile, 256);
			GetDlgItemText(hWnd, ID_ROM_NFA, opt_cartfn[0], 256);
			GetDlgItemText(hWnd, ID_ADR_NFA, tmpfield, 256);
			sscanf(tmpfield,"%x",&opt_cartaddr[0]);
			GetDlgItemText(hWnd, ID_ROM_OTHER, opt_cartfn[1], 256);
			GetDlgItemText(hWnd, ID_ADR_OTHER, tmpfield, 256);
			sscanf(tmpfield,"%x",&opt_cartaddr[1]);

			GetDlgItemText(hWnd, ID_MDV1, mdvfn[0], 256);
			GetDlgItemText(hWnd, ID_MDV2, mdvfn[1], 256);

			GetDlgItemText(hWnd, ID_WIN1, winfn[0], 256);
			GetDlgItemText(hWnd, ID_WIN2, winfn[1], 256);

			GetDlgItemText(hWnd, ID_CBX_SS, tmpfield, 256);
			screen_res=tmpfield[0]-'0';

			opt=-1;
			GetDlgItemText(hWnd, ID_CBX_MS, tmpfield, 256);
			tmpfield[2]='\0';
			if (strcmp(tmpfield,"12")==0) opt=0;
			if (strcmp(tmpfield,"64")==0) opt=-1;
			if (strcmp(tmpfield,"1M")==0) opt=1;
			if (strcmp(tmpfield,"2M")==0) opt=2;
			if (strcmp(tmpfield,"4M")==0) opt=4;
			if (strcmp(tmpfield,"8M")==0) opt=8;
			ram_size=opt;

			opt=-1;
			GetDlgItemText(hWnd, ID_CBX_KBD, tmpfield, 256);
			tmpfield[2]='\0';
			if (strcmp(tmpfield,"US")==0) opt=KEYB_US;
			if (strcmp(tmpfield,"UK")==0) opt=KEYB_UK;
			if (strcmp(tmpfield,"GE")==0) opt=KEYB_GE;
			if (strcmp(tmpfield,"FR")==0) opt=KEYB_FR;
			if (strcmp(tmpfield,"IT")==0) opt=KEYB_IT;
			if (strcmp(tmpfield,"SE")==0) opt=KEYB_SE;
			opt_keyb=opt;

			GetDlgItemText(hWnd, ID_SPEED, tmpfield, 256);
			sscanf(tmpfield,"%d",&qlay1msec);
			GetDlgItemText(hWnd, ID_DELAY, tmpfield, 256);
			sscanf(tmpfield,"%d",&opt_busy_wait);

			if (writefile) write_options();

			EndDialog(hWnd, TRUE);
			return TRUE;

		case ID_LOAD:

		case ID_CANCEL:
			EndDialog(hWnd, FALSE);
			return TRUE;

		case ID_BRW_BOOT:
		case ID_BRW_NFA:
		case ID_BRW_OTHER:
		case ID_BRW_WIN1:
		case ID_BRW_WIN2:
		case ID_BRW_MDV1:
		case ID_BRW_MDV2:

			if (! *szDirName){
				szDirName[0]='.';
				szDirName[1]='\0';
			}

			*szFile = '\0';
			memset(&ofn, 0, sizeof(ofn));

			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = hWnd;
			ofn.hInstance = hInstance;
			ofn.nFilterIndex = 1;
			ofn.lpstrFile = szFile;
			ofn.nMaxFile = sizeof(szFile);
			ofn.lpstrInitialDir = szDirName;
//			ofn.Flags = OFN_SHOWHELP | OFN_FILEMUSTEXIST;
			ofn.Flags = OFN_HIDEREADONLY |
				OFN_FILEMUSTEXIST |
				OFN_NOCHANGEDIR;	/* come back in cwd */
			if (wPar == ID_BRW_BOOT) {
				strcpy(ofn.lpstrFile,romfile);
				ofn.lpstrInitialDir = gcd(romfile);
				ofn.lpstrFilter = "ROM Files (*.rom)\0*.rom\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select Boot ROM File";
				id = ID_ROM_BOOT;
				bRet = GetOpenFileName(&ofn);
			}
			else if (wPar == ID_BRW_NFA) {
				strcpy(ofn.lpstrFile,opt_cartfn[0]);
				ofn.lpstrInitialDir = gcd(opt_cartfn[0]);
				ofn.lpstrFilter = "ROM Files (*.rom)\0*.rom\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select NFA ROM file";
				id = ID_ROM_NFA;
				bRet = GetOpenFileName(&ofn);
			}
			else if (wPar == ID_BRW_OTHER) {
				strcpy(ofn.lpstrFile,opt_cartfn[1]);
				ofn.lpstrInitialDir = gcd(opt_cartfn[1]);
				ofn.lpstrFilter = "ROM Files (*.rom)\0*.rom\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select other ROM filename";
				id = ID_ROM_OTHER;
				bRet = GetOpenFileName(&ofn);
			}
			else if (wPar == ID_BRW_WIN1) {
				strcpy(ofn.lpstrFile,winfn[0]);
				ofn.lpstrInitialDir = gcd(winfn[0]);
				ofn.lpstrFilter = "QLAY.DIR Files (qlay.dir)\0qlay.dir\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select QLAY.DIR file";
				id = ID_WIN1;
				bRet = GetOpenFileName(&ofn);
			}
			else if (wPar == ID_BRW_WIN2) {
				strcpy(ofn.lpstrFile,winfn[1]);
				ofn.lpstrInitialDir = gcd(winfn[1]);
				ofn.lpstrFilter = "QLAY.DIR Files (qlay.dir)\0qlay.dir\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select QLAY.DIR file";
				id = ID_WIN2;
				bRet = GetOpenFileName(&ofn);
			}
			else if (wPar == ID_BRW_MDV1) {
				strcpy(ofn.lpstrFile,mdvfn[0]);
				ofn.lpstrInitialDir = gcd(mdvfn[0]);
				ofn.lpstrFilter = "MDV Files (*.mdv)\0*.mdv\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select MDV file";
				id = ID_MDV1;
				bRet = GetOpenFileName(&ofn);
			}
			else /*if (wPar == ID_BRW_MDV2) */{
				strcpy(ofn.lpstrFile,mdvfn[1]);
				ofn.lpstrInitialDir = gcd(mdvfn[1]);
				ofn.lpstrFilter = "MDV Files (*.mdv)\0*.mdv\0All files (*.*)\0*.*\0";
				ofn.lpstrTitle = "Select MDV file";
				id = ID_MDV2;
				bRet = GetOpenFileName(&ofn);
			}

			if (bRet) {
				char *s;

				/* display browsed file */
				SetDlgItemText(hWnd, id, szFile);

				/* set standard Directory Path */
				strcpy(szDirName, szFile);
				s = szDirName + strlen(szDirName);
				while (s > szDirName && *--s != '\\')
				    ;
				*s = '\0';
			}
			return TRUE;

		default:
			return FALSE;


            } /* switch (wPar) */

        case WM_SYSCOMMAND :
            if (wPar == SC_CLOSE) {
		EndDialog(hWnd, FALSE);
		return TRUE;
            }
            else
		return FALSE;

        default:
            return FALSE;

    } /* switch (wMsg)*/

}

int wincfg_main(HINSTANCE hInst)
{
DLGPROC	qlcfg_dialog;

	hInstance = hInst;
	qlcfg_dialog = (DLGPROC) MakeProcInstance((FARPROC) Dialog, hInst);
	DialogBox(hInst, (LPSTR) "CFG_DLG", NULL, qlcfg_dialog);
	FreeProcInstance((FARPROC) qlcfg_dialog);

	return 0;
}
