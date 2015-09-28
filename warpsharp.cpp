/*********************************************************************
* 	WarpSharp for AviUtl
* 								ver. 0.01
* 
* AviSynth版のWarpSharpがGPLらしいので、これもGPLになります。
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
*********************************************************************/
#include <windows.h>
#include <math.h>
#include "filter.h"

//--------------------------------------------------------------------
//	class Cubic4Table	warpsharp.hより
//--------------------------------------------------------------------
class Cubic4Table {
	int table[1024];

public:
	Cubic4Table(int a) {
		double A = (double)a / 10;
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



//----------------------------
//	グローバル変数
//----------------------------
const int radius = 2;
const int kernel = radius * 2 + 1;


//----------------------------
//	プロトタイプ
//----------------------------
void  Bump(short *dst,const PIXEL_YC *src,const FILTER_PROC_INFO* fpip,int bump);
void  Blur(short *dst,short** row,short*(*tbl)[kernel],const FILTER_PROC_INFO* fpip);
void  BlurRow(short* d,short* s,int w);
void  BlurCol(short* d,short** tbl,int w);
inline int InterpolateCubic(const PIXEL_YC* src,int pitch,const int *qh,const int *qv);
inline int AviUtlY(int y){ return ((y-15)*16 * 256 +110) / 220; }



//----------------------------
//	FILTER_DLL構造体
//----------------------------
char filtername[] = "WarpSharp";
char filterinfo[] = "WarpSharp for AviUtl ver 0.01 Transplant by MakKi";
#define track_N 4
#if track_N
TCHAR *track_name[]   = { "depth", "blur", "bump", "cubic" };	// トラックバーの名前
int   track_default[] = {    128 ,     3 ,   128 ,     -6  };	// トラックバーの初期値
int   track_s[]       = {      0 ,     1 ,     0 ,    -50  };	// トラックバーの下限値
int   track_e[]       = {    256 ,     9 ,   256 ,     50  };	// トラックバーの上限値
#endif

#define check_N 0
#if check_N
TCHAR *check_name[]   = { 0 };	// チェックボックス
int   check_default[] = { 0, 0 };	// デフォルト
#endif

#define tDEPTH  0
#define tBLUR   1
#define tBUMP   2
#define tCUBIC  3


FILTER_DLL filter = {
	FILTER_FLAG_EX_INFORMATION,
	NULL,NULL,      	// 設定ウインドウのサイズ
	filtername,     	// フィルタの名前
	track_N,        	// トラックバーの数
#if track_N
	track_name,     	// トラックバーの名前郡
	track_default,  	// トラックバーの初期値郡
	track_s,track_e,	// トラックバーの数値の下限上限
#else
	NULL,NULL,NULL,NULL,
#endif
	check_N,      	// チェックボックスの数
#if check_N
	check_name,   	// チェックボックスの名前郡
	check_default,	// チェックボックスの初期値郡
#else
	NULL,NULL,
#endif
	func_proc,   	// フィルタ処理関数
	NULL,NULL,   	// 開始時,終了時に呼ばれる関数
	NULL,        	// 設定が変更されたときに呼ばれる関数
	func_WndProc,	// 設定ウィンドウプロシージャ
	NULL,NULL,   	// システムで使用
	NULL,NULL,   	// 拡張データ領域
	filterinfo,  	// フィルタ情報
	NULL,			// セーブ開始直前に呼ばれる関数
	NULL,			// セーブ終了時に呼ばれる関数
	NULL,NULL,NULL,	// システムで使用
	NULL,			// 拡張領域初期値
};

/*********************************************************************
*	DLL Export
*********************************************************************/
EXTERN_C FILTER_DLL __declspec(dllexport) * __stdcall GetFilterTable( void )
{
	return &filter;
}

/*====================================================================
*	フィルタ処理関数
*===================================================================*/
BOOL func_proc(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	int i;
	short *bump = new short[fpip->w*fpip->h];
	short *bum  = bump;

	// 輪郭抽出
	Bump(bum,fpip->ycp_edit,fpip,fp->track[tBUMP]*16);

	// ぼかし用作業領域
	int   rowsize = (fpip->w+15)&~15;
	short *buf = new short[rowsize*fpip->h * kernel +16];
	short *row[kernel];
	short *tbl[kernel][kernel];
	row[0] = (short*)(((long)buf+15)&~15);
	for(i=1;i<kernel;++i)
		row[i] = row[i-1] + rowsize;
	for(i=0;i<kernel;++i)
		for(int j=0;j<kernel;++j)
			tbl[i][j] = row[(i + j + 1) % kernel];

	for(i=fp->track[tBLUR];i;--i)	// 指定回ぼかす
		Blur(bum,row,tbl,fpip);

	// Cubic
	Cubic4Table cubic(fp->track[tCUBIC]);

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
			int *qh = (int*)cubic + (dispx & 255) * 4;
			int *qv = (int*)cubic + (dispy & 255) * 4;

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

		lo_dispy -= 255;
		hi_dispy -= 255;
	}
	memcpy(dst,src,fpip->w*sizeof(*dst));

	dst = fpip->ycp_edit;
	fpip->ycp_edit = fpip->ycp_temp;
	fpip->ycp_temp = dst;

	delete[] bump;
	delete[] buf;

	return TRUE;
}

/*--------------------------------------------------------------------
*	Bump	輪郭抽出
*-------------------------------------------------------------------*/
void Bump(short *dst,const PIXEL_YC *src,const FILTER_PROC_INFO* fpip,int bump)
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
*	Blur	輪郭ぼかし
*-------------------------------------------------------------------*/
void Blur(short *dst,short** row,short*(*tbl)[kernel],const FILTER_PROC_INFO* fpip)
{
	BlurRow(row[0], dst, fpip->w);
	memcpy(row[1],row[0],fpip->w*sizeof(**row));
	memcpy(row[2],row[0],fpip->w*sizeof(**row));
	BlurRow(row[3], dst+fpip->w, fpip->w);

	int i = kernel-1;

	for(int h=fpip->h-radius;h;--h){
		BlurRow(row[i], dst+fpip->w*2, fpip->w);
		BlurCol(dst,tbl[i],fpip->w);

		if(++i>=kernel) i = 0;

		dst += fpip->w;
	}

	memcpy(row[i],row[i?i-1:kernel-1],fpip->w*sizeof(**row));
	BlurCol(dst,tbl[i],fpip->w);
	if(++i>=kernel) i = 0;
	memcpy(row[i],row[i?i-1:kernel-1],fpip->w*sizeof(**row));
	BlurCol(dst,tbl[i],fpip->w);
}
/*--------------------------------------------------------------------
*	BlurRow	横ぼかし
*-------------------------------------------------------------------*/
void BlurRow(short* d,short *s,int w)
{
	d[0] = (s[0]*11 + s[1]*4 + s[2]          +8) >>4;
	d[1] = (s[0]*5  + s[1]*6 + s[2]*4 + s[3] +8) >>4;

	for(w-=4,d+=2;w;--w,++d,++s)
		*d = (s[0] + s[1]*4 + s[2]*6 + s[3]*4 + s[4] +8) >>4;

	d[0] = (s[0] + s[1]*4 + s[2]*6 + s[3]*5 +8) >>4;
	d[1] = (s[0] + s[1]*4 + s[2]*11         +8) >>4;
}

/*--------------------------------------------------------------------
*	BlurCol	縦ぼかし
*-------------------------------------------------------------------*/
void BlurCol(short* d,short** tbl,int w)
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

	luma  = ((p0[0].y*qh[0] + p0[1].y*qh[1] + p0[2].y*qh[2] + p0[3].y*qh[3] + 512) >>10) * qv[0];
	luma += ((p1[0].y*qh[0] + p1[1].y*qh[1] + p1[2].y*qh[2] + p1[3].y*qh[3] + 512) >>10) * qv[1];
	luma += ((p2[0].y*qh[0] + p2[1].y*qh[1] + p2[2].y*qh[2] + p2[3].y*qh[3] + 512) >>10) * qv[2];
	luma += ((p3[0].y*qh[0] + p3[1].y*qh[1] + p3[2].y*qh[2] + p3[3].y*qh[3] + 512) >>10) * qv[3];
	luma = (luma + (1<<17)) >> 18;

	if(luma<AviUtlY(0)) luma = AviUtlY(0);
	else if(luma>AviUtlY(255)) luma = AviUtlY(255);

	return luma;
}

/*====================================================================
*	設定ウィンドウプロシージャ
*===================================================================*/
BOOL func_WndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp )
{
	switch(message){
		case WM_KEYUP:	// メインウィンドウへ送る
		case WM_KEYDOWN:
//		case WM_MOUSEWHEEL:
			SendMessage(GetWindow(hwnd, GW_OWNER), message, wparam, lparam);
			break;
	}

	return FALSE;
}



