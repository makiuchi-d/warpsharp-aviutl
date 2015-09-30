/*********************************************************************
* 	WarpSharp for AviUtl
* 								ver. 0.03
* 
* AviSynth�ł�WarpSharp��GPL�炵���̂ŁA�����GPL�ɂȂ�܂��B
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
* 
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
* http://www.gnu.org/copyleft/gpl.html .
* 
* 
* [2003]
* 	10/19:	���J(0.01)
* 			�ŉ��[�ɃS�~���o��o�O�C��(0.01a)
* [2004]
* 	01/10:	Cubic4Table�ł̖��ʂȍČv�Z�����
* 	01/12:	�K���͈͂̐ݒ��ǉ�(0.02)
* 	01/26:	�K���͈̓p�����[�^�̍ő�l���g��
* 	12/13:	01/10�̏C�����s���S�������̂��C��
* [2008]
* 	06/28:	�ڂ����ȃ������d�l�̂͂����A�������m�ۗʂ��̂܂܂������~�X�C��
* 			Cubic4Table�Čv�Z�͊��ɉ������Ă�(041213)���ǁA�����func_update�Ɉړ��B
* 			��������bump��depth��256�ŃI�[�o�[�t���[���Ȃ���B
* 			�ڂ��������ŉ��[����ʂ茩���������Ǒ��v�Ȃ͂��B
* 			�K���͈̓p�����[�^��傫���������鎞�̖���040126�ɏC�����Ă��݂����B
* 			�X�V���Y��Ă܂����B���߂�Ȃ����B(0.03)
* 
*********************************************************************/
#include <windows.h>
#include <math.h>
#include "filter.h"

//--------------------------------------------------------------------
//	class Cubic4Table	warpsharp.h���
//--------------------------------------------------------------------
class Cubic4Table {
	int table[1024];
	int a;

public:
	Cubic4Table(int _a =-6)
	{
		a = _a+1;
		SetTable(_a);
	}

	void SetTable(int _a)
	{
		if(a==_a) return;

		a = _a;
	
		double A = (double)_a / 10;
		for(int i=0; i<256; i++) {
			double d = (double)i / 256.0;
			int y1, y2, y3, y4, ydiff;

			// Coefficients for all four pixels *must* add up to 1.0 for
			// consistent unity gain.
			//
			// Two good values for A are -1.0 (original VirtualDub bicubic filter)
			// and -0.75 (closely matches Photoshop).

			y1 = (int)floor(0.5 + (      + A*d -       2.0*A*d*d +       A*d*d*d) * 16384.0);
			y2 = (int)floor(0.5 + (+ 1.0       -     (A+3.0)*d*d + (A+2.0)*d*d*d) * 16384.0);
			y3 = (int)floor(0.5 + (      - A*d + (2.0*A+3.0)*d*d - (A+2.0)*d*d*d) * 16384.0);
			y4 = (int)floor(0.5 + (            +           A*d*d -       A*d*d*d) * 16384.0);

			// Normalize y's so they add up to 16384.

			ydiff = (16384 - y1 - y2 - y3 - y4)/4;
//			assert(ydiff > -16 && ydiff < 16);

			y1 += ydiff;
			y2 += ydiff;
			y3 += ydiff;
			y4 += ydiff;

//			if (mmx_table) {
//				table[i*4 + 0] = (y2<<16) | (y1 & 0xffff);
//				table[i*4 + 1] = (y4<<16) | (y3 & 0xffff);
//				table[i*4 + 2] = table[i*4 + 3] = 0;
//			} else {
				table[i*4 + 0] = y1;
				table[i*4 + 1] = y2;
				table[i*4 + 2] = y3;
				table[i*4 + 3] = y4;
//			}
		}
	}

	const int *GetTable() const
	{ return table; }

	operator int* (void)
	{ return table; }
};

//--------------------------------------------------------------------
//	class DefaultPicture
//--------------------------------------------------------------------
class DefaultPicture {
	PIXEL_YC* ycp_edit;
	PIXEL_YC* ycp_temp;
	int       h,w;
public:
	DefaultPicture(){  }
	DefaultPicture(const FILTER_PROC_INFO* fpip)
	{
		ycp_edit = fpip->ycp_edit;
		ycp_temp = fpip->ycp_temp;
		h = fpip->h;
		w = fpip->w;
	}

	void Store(FILTER_PROC_INFO* fpip)
	{
		fpip->ycp_edit = ycp_edit;
		fpip->ycp_temp = ycp_temp;
		fpip->h = h;
		fpip->w = w;
	}
	void ExchangeStore(FILTER_PROC_INFO* fpip)
	{
		fpip->ycp_edit = ycp_temp;
		fpip->ycp_temp = ycp_edit;
		fpip->h = h;
		fpip->w = w;
	}
};


//----------------------------
//	�O���[�o���ϐ�
//----------------------------

// �萔
const int radius = 2;	// �ڂ������a
const int kernel = radius * 2 + 1;	// �ڂ����͈�

static Cubic4Table cubic;


//----------------------------
//	�v���g�^�C�v
//----------------------------
inline int InterpolateCubic(const PIXEL_YC* src,int pitch,const int *qh,const int *qv);
inline int AviUtlY(int y){ return ((y-15)*16 * 256 +110) / 220; }
static void  Bump(short *dst,const PIXEL_YC *src,const FILTER_PROC_INFO* fpip,int bump);
static void  Blur(short *dst,short** row,short*(*tbl)[kernel],const FILTER_PROC_INFO* fpip);
static void  BlurRow(short* d,short* s,int w);
static void  BlurCol(short* d,short** tbl,int w);
static BOOL NegaPosi(FILTER *fp,FILTER_PROC_INFO *fpip);
static void  CopyEdge(FILTER *fp,FILTER_PROC_INFO *fpip);



//----------------------------
//	FILTER_DLL�\����
//----------------------------
char filtername[] = "WarpSharp";
char filterinfo[] = "WarpSharp for AviUtl ver 0.03 Transplanted by MakKi";
#define track_N 8
#if track_N
TCHAR *track_name[]   = { "depth", "blur", "bump", "cubic", "��", "��", "��", "�E"};	// �g���b�N�o�[�̖��O
int   track_default[] = {    128 ,     3 ,   128 ,     -6 ,   0 ,   0 ,   0 ,   0 };	// �g���b�N�o�[�̏����l
int   track_s[]       = {      0 ,     1 ,     0 ,    -50 ,   0 ,   0 ,   0 ,   0 };	// �g���b�N�o�[�̉����l
int   track_e[]       = {    256 ,     9 ,   256 ,     50 , 512 , 512 , 512 , 512 };	// �g���b�N�o�[�̏���l
#endif

#define check_N 1
#if check_N
TCHAR *check_name[]   = { "�͈͕\��" };	// �`�F�b�N�{�b�N�X
int   check_default[] = {  0 };	// �f�t�H���g
#endif

#define tDEPTH  0
#define tBLUR   1
#define tBUMP   2
#define tCUBIC  3
#define tTOP    4
#define tBTM    5
#define tLEFT   6
#define tRIGHT  7
#define cDISP   0


FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	NULL,NULL,      	// �ݒ�E�C���h�E�̃T�C�Y
	filtername,     	// �t�B���^�̖��O
	track_N,        	// �g���b�N�o�[�̐�
#if track_N
	track_name,     	// �g���b�N�o�[�̖��O�S
	track_default,  	// �g���b�N�o�[�̏����l�S
	track_s,track_e,	// �g���b�N�o�[�̐��l�̉������
#else
	NULL,NULL,NULL,NULL,
#endif
	check_N,      	// �`�F�b�N�{�b�N�X�̐�
#if check_N
	check_name,   	// �`�F�b�N�{�b�N�X�̖��O�S
	check_default,	// �`�F�b�N�{�b�N�X�̏����l�S
#else
	NULL,NULL,
#endif
	func_proc,   	// �t�B���^�����֐�
	NULL,NULL,   	// �J�n��,�I�����ɌĂ΂��֐�
	func_update, 	// �ݒ肪�ύX���ꂽ�Ƃ��ɌĂ΂��֐�
	func_WndProc,	// �ݒ�E�B���h�E�v���V�[�W��
	NULL,NULL,   	// �V�X�e���Ŏg�p
	NULL,NULL,   	// �g���f�[�^�̈�
	filterinfo,  	// �t�B���^���
	NULL,			// �Z�[�u�J�n���O�ɌĂ΂��֐�
	NULL,			// �Z�[�u�I�����ɌĂ΂��֐�
	NULL,NULL,NULL,	// �V�X�e���Ŏg�p
	NULL,			// �g���̈揉���l
};

/*********************************************************************
*	DLL Export
*********************************************************************/
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable( void )
{
	return &filter;
}

//---------------------------------------------------------------------
//		�t�B���^�ݒ�ύX���֐�
//---------------------------------------------------------------------
BOOL func_update( FILTER *fp, int status )
{
	// Cubic
	cubic.SetTable(fp->track[tCUBIC]);
	
	return TRUE;
}

/*====================================================================
*	�t�B���^�����֐�
*===================================================================*/
BOOL func_proc(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	DefaultPicture defpic(fpip);

	if(fp->track[tTOP] || fp->track[tBTM] || fp->track[tLEFT] || fp->track[tRIGHT]){
		// �͈͎w�肳��Ă���
		if(fp->track[tTOP]+fp->track[tBTM]>=fpip->h-3 || fp->track[tLEFT]+fp->track[tRIGHT]>=fpip->w-3)
			return FALSE;
		CopyEdge(fp,fpip);	// �͈͊O���R�s�[
		fpip->ycp_edit += fp->track[tLEFT] + fp->track[tTOP] * fpip->max_w;
		fpip->ycp_temp += fp->track[tLEFT] + fp->track[tTOP] * fpip->max_w;
		fpip->w        -= fp->track[tLEFT] + fp->track[tRIGHT];
		fpip->h        -= fp->track[tTOP]  + fp->track[tBTM];
	}

	if(fp->check[cDISP]){	// �K�p�͈͕\��
		NegaPosi(fp,fpip);	// NegativePositive
		defpic.Store(fpip);
		return TRUE;
	}


	int i;
	short *bump = new short[fpip->w*fpip->h];
	short *bum  = bump;

	// �֊s���o
	Bump(bum,fpip->ycp_edit,fpip,fp->track[tBUMP]*16);

	// �ڂ����p��Ɨ̈�
	int   rowsize = (fpip->w+15)&~15;
	short *buf = new short[rowsize * kernel +16];	// �ȃ������ɂ��Y��Ă�
	short *row[kernel];
	short *tbl[kernel][kernel];
	row[0] = (short*)(((long)buf+15)&~15);
	for(i=1;i<kernel;++i)
		row[i] = row[i-1] + rowsize;
	for(i=0;i<kernel;++i)
		for(int j=0;j<kernel;++j)
			tbl[i][j] = row[(i + j + 1) % kernel];

	for(i=fp->track[tBLUR];i;--i)	// �w���ڂ���
		Blur(bum,row,tbl,fpip);

	delete[] buf;

	// WarpSharp
	int depth = fp->track[tDEPTH];
	PIXEL_YC* dst = fpip->ycp_temp;
	const PIXEL_YC* src = fpip->ycp_edit;

	memcpy(dst,src,fpip->w*sizeof(*dst));
	dst += fpip->max_w;
	src += fpip->max_w;
	bum += fpip->w;

	int lo_dispy = 0;
	int hi_dispy = (fpip->h - 3) *256 -1;

	for(int h=fpip->h-2; h; --h){
		*dst = *src;
		bum+=1; ++dst; ++src;

		int lo_dispx = 0;
		int hi_dispx = (fpip->w - 3) *256 -1;

		for(int w=fpip->w-2; w; --w){
			int dispx = bum[-1] - bum[1];
			int dispy = bum[-fpip->w] - bum[fpip->w];

			dispx = (dispx * depth +128) >>8;// (dispx * depth +8) >>4;
			dispy = (dispy * depth +128) >>8;

			if(dispx < lo_dispx)      dispx = lo_dispx;
			else if(dispx > hi_dispx) dispx = hi_dispx;

			if(dispy < lo_dispy)      dispy = lo_dispy;
			else if(dispy > hi_dispy) dispy = hi_dispy;

			const PIXEL_YC* src2 = src + ((dispx>>8)-1) + ((dispy>>8)-1)*fpip->max_w;
			const int *qh = (int*)cubic + (dispx & 255) * 4;
			const int *qv = (int*)cubic + (dispy & 255) * 4;

			dst->y = InterpolateCubic(src2,fpip->max_w,qh,qv);
			dst->cb = src->cb;
			dst->cr = src->cr;

			++dst; ++src; ++bum;
			lo_dispx -= 256;
			hi_dispx -= 256;
		}

		*dst = *src;

		dst += fpip->max_w - fpip->w +1;
		src += fpip->max_w - fpip->w +1;
		++bum;

		lo_dispy -= 256;
		hi_dispy -= 256;
	}
	memcpy(dst,src,fpip->w*sizeof(*dst));

	defpic.ExchangeStore(fpip);

/* test *
	for(i=0;i<fpip->h;i++){
		for(int j=0;j<fpip->w;j++){
			fpip->ycp_edit[i*fpip->max_w+j].y = bump[i*fpip->w+j];
		}
	}

//*/
	delete[] bump;

	return TRUE;
}

/*--------------------------------------------------------------------
*	Bump	�֊s���o
*-------------------------------------------------------------------*/
static void Bump(short *dst,const PIXEL_YC *src,const FILTER_PROC_INFO* fpip,int bump)
{
	memset(dst,0,fpip->w*sizeof(*dst));
	dst += fpip->w;
	src += fpip->max_w;

	for(int i=fpip->h-2;i;--i){
		*dst = 0;

		dst ++;
		src ++;
		for(int j=fpip->w-2;j;--j){
			int dx,dy;

			dx = abs(src[-1-fpip->max_w].y + src[-1].y + src[-1+fpip->max_w].y
					- src[1-fpip->max_w].y - src[1].y - src[1+fpip->max_w].y);
			dy = abs(src[-fpip->max_w-1].y + src[-fpip->max_w].y + src[-fpip->max_w+1].y
					- src[fpip->max_w-1].y - src[fpip->max_w].y - src[fpip->max_w+1].y);

			int level = 3*(dx+dy) + abs(dx-dy);

			*dst = (level>bump) ? bump : level;

			dst += 1;
			src += 1;
		}
		*dst = 0;

		src += fpip->max_w - fpip->w +1;
		dst += 1;
	}
	memset(dst,0,fpip->w*sizeof(*dst));
}

/*--------------------------------------------------------------------
*	Blur	�֊s�ڂ���
*
* 2�s���܂ŉ��ɂڂ�������c�ɂڂ�����B
* �ڂ����p�o�b�t�@��5�s���B�g���܂킷�B
*-------------------------------------------------------------------*/
static void Blur(short *dst,short** row,short*(*tbl)[kernel],const FILTER_PROC_INFO* fpip)
{
	// radius = 2, kernel = 5
	BlurRow(row[0], dst, fpip->w);
	memcpy(row[1],row[0],fpip->w*sizeof(**row));
	memcpy(row[2],row[0],fpip->w*sizeof(**row));
	BlurRow(row[3], dst+fpip->w, fpip->w);

	int i = kernel - 1;

	for(int h=fpip->h-radius;h;--h){
		BlurRow(row[i], dst+fpip->w*radius, fpip->w);
		BlurCol(dst,tbl[i],fpip->w);

		if(++i>=kernel) i = 0;

		dst += fpip->w;
	}

	memcpy(row[i],row[i?i-1:kernel-1],fpip->w*sizeof(**row));
	BlurCol(dst,tbl[i],fpip->w);
	if(++i>=kernel) i = 0;
	dst += fpip->w;
	memcpy(row[i],row[i?i-1:kernel-1],fpip->w*sizeof(**row));
	BlurCol(dst,tbl[i],fpip->w);
}
/*--------------------------------------------------------------------
*	BlurRow	���ڂ���
*-------------------------------------------------------------------*/
static void BlurRow(short* d,short *s,int w)
{
	d[0] = (s[0]*11 + s[1]*4 + s[2]          +8) >>4;
	d[1] = (s[0]*5  + s[1]*6 + s[2]*4 + s[3] +8) >>4;

	for(w-=4,d+=2;w;--w,++d,++s)
		*d = (s[0] + s[1]*4 + s[2]*6 + s[3]*4 + s[4] +8) >>4;

	d[0] = (s[0] + s[1]*4 + s[2]*6 + s[3]*5 +8) >>4;
	d[1] = (s[0] + s[1]*4 + s[2]*11         +8) >>4;
}

/*--------------------------------------------------------------------
*	BlurCol	�c�ڂ���
*-------------------------------------------------------------------*/
static void BlurCol(short* d,short** tbl,int w)
{
	int x = 0;

	for(;x<w;++x)
		d[x] = (tbl[0][x] + tbl[1][x]*4 + tbl[2][x]*6 + tbl[3][x]*4 + tbl[4][x] +8)>>4;
}

/*--------------------------------------------------------------------
*	InterpolateCubic
*-------------------------------------------------------------------*/
inline int InterpolateCubic(const PIXEL_YC* src,int pitch,const int *qh,const int *qv)
{
	const PIXEL_YC* p0 = src;
	const PIXEL_YC* p1 = p0 + pitch;
	const PIXEL_YC* p2 = p1 + pitch;
	const PIXEL_YC* p3 = p2 + pitch;
	int luma;

	luma  = ((p0[0].y*qh[0] + p0[1].y*qh[1] + p0[2].y*qh[2] + p0[3].y*qh[3] + 1024) >>11) * qv[0];
	luma += ((p1[0].y*qh[0] + p1[1].y*qh[1] + p1[2].y*qh[2] + p1[3].y*qh[3] + 1024) >>11) * qv[1];
	luma += ((p2[0].y*qh[0] + p2[1].y*qh[1] + p2[2].y*qh[2] + p2[3].y*qh[3] + 1024) >>11) * qv[2];
	luma += ((p3[0].y*qh[0] + p3[1].y*qh[1] + p3[2].y*qh[2] + p3[3].y*qh[3] + 1024) >>11) * qv[3];
	luma = (luma + (1<<16)) >> 17;

	if(luma<AviUtlY(0)) luma = AviUtlY(0);
	else if(luma>AviUtlY(255)) luma = AviUtlY(255);

	return luma;
}

/*--------------------------------------------------------------------
*	NegaPosi	�l�K�|�W
*-------------------------------------------------------------------*/
static BOOL NegaPosi(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	PIXEL_YC* ptr1 = fpip->ycp_edit;

	for(int i=fpip->h; i; --i){
		PIXEL_YC* ptr2 = ptr1;
		for(int j=fpip->w; j; --j){
			ptr2->y  = 4096 - ptr2->y;
			ptr2->cb = -ptr2->cb;
			ptr2->cr = -ptr2->cr;
			++ptr2;
		}
		ptr1 += fpip->max_w;
	}

	return TRUE;
}

/*--------------------------------------------------------------------
*	CopyEdge	�͈͂̊O���R�s�[
*-------------------------------------------------------------------*/
static void CopyEdge(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	PIXEL_YC* src = fpip->ycp_edit;
	PIXEL_YC* dst = fpip->ycp_temp;
	int i;

	int rowsize = fpip->w*sizeof(PIXEL_YC);

	for(i=fp->track[tTOP];i;--i){
		memcpy(dst,src,rowsize);
		src += fpip->max_w;
		dst += fpip->max_w;
	}
	int rightpitch = fpip->w - fp->track[tRIGHT];
	int right = fp->track[tRIGHT]*sizeof(PIXEL_YC);
	int left  = fp->track[tLEFT]*sizeof(PIXEL_YC);
	for(i=fpip->h-fp->track[tTOP]-fp->track[tBTM];i;--i){
		PIXEL_YC* src2 = src;
		PIXEL_YC* dst2 = dst;
		memcpy(dst2,src2,left);
		src2 += rightpitch;
		dst2 += rightpitch;
		memcpy(dst2,src2,right);
		src += fpip->max_w;
		dst += fpip->max_w;
	}
	for(i=fp->track[tBTM];i;--i){
		memcpy(dst,src,rowsize);
		src += fpip->max_w;
		dst += fpip->max_w;
	}

	return;
}

/*====================================================================
*	�ݒ�E�B���h�E�v���V�[�W��
*===================================================================*/
BOOL func_WndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp )
{
	switch(message){
		case WM_KEYUP:	// ���C���E�B���h�E�֑���
		case WM_KEYDOWN:
		case WM_MOUSEWHEEL:
			SendMessage(GetWindow(hwnd, GW_OWNER), message, wparam, lparam);
			break;
	}

	return FALSE;
}


