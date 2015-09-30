/*********************************************************************
* 	WarpSharp for AviUtl
* 								ver. 0.03
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
* 
* [2003]
* 	10/19:	公開(0.01)
* 			最下端にゴミが出るバグ修正(0.01a)
* [2004]
* 	01/10:	Cubic4Tableでの無駄な再計算を回避
* 	01/12:	適応範囲の設定を追加(0.02)
* 	01/26:	適応範囲パラメータの最大値を拡張
* 	12/13:	01/10の修正が不完全だったのを修正
* [2008]
* 	06/28:	ぼかし省メモリ仕様のはずが、メモリ確保量そのままだったミス修正
* 			Cubic4Table再計算は既に回避されてた(041213)けど、さらにfunc_updateに移動。
* 			こっちはbumpもdepthも256でオーバーフローしないよ。
* 			ぼかし処理最下端も一通り見直したけど大丈夫なはず。
* 			適応範囲パラメータを大きくしすぎる時の問題も040126に修正してたみたい。
* 			更新し忘れてました。ごめんなさい。(0.03)
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
//	グローバル変数
//----------------------------

// 定数
const int radius = 2;	// ぼかし半径
const int kernel = radius * 2 + 1;	// ぼかし範囲

static Cubic4Table cubic;


//----------------------------
//	プロトタイプ
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
//	FILTER_DLL構造体
//----------------------------
char filtername[] = "WarpSharp";
char filterinfo[] = "WarpSharp for AviUtl ver 0.03 Transplanted by MakKi";
#define track_N 8
#if track_N
TCHAR *track_name[]   = { "depth", "blur", "bump", "cubic", "上", "下", "左", "右"};	// トラックバーの名前
int   track_default[] = {    128 ,     3 ,   128 ,     -6 ,   0 ,   0 ,   0 ,   0 };	// トラックバーの初期値
int   track_s[]       = {      0 ,     1 ,     0 ,    -50 ,   0 ,   0 ,   0 ,   0 };	// トラックバーの下限値
int   track_e[]       = {    256 ,     9 ,   256 ,     50 , 512 , 512 , 512 , 512 };	// トラックバーの上限値
#endif

#define check_N 1
#if check_N
TCHAR *check_name[]   = { "範囲表示" };	// チェックボックス
int   check_default[] = {  0 };	// デフォルト
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
	func_update, 	// 設定が変更されたときに呼ばれる関数
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

//---------------------------------------------------------------------
//		フィルタ設定変更時関数
//---------------------------------------------------------------------
BOOL func_update( FILTER *fp, int status )
{
	// Cubic
	cubic.SetTable(fp->track[tCUBIC]);
	
	return TRUE;
}

/*====================================================================
*	フィルタ処理関数
*===================================================================*/
BOOL func_proc(FILTER *fp,FILTER_PROC_INFO *fpip)
{
	DefaultPicture defpic(fpip);

	if(fp->track[tTOP] || fp->track[tBTM] || fp->track[tLEFT] || fp->track[tRIGHT]){
		// 範囲指定されている
		if(fp->track[tTOP]+fp->track[tBTM]>=fpip->h-3 || fp->track[tLEFT]+fp->track[tRIGHT]>=fpip->w-3)
			return FALSE;
		CopyEdge(fp,fpip);	// 範囲外をコピー
		fpip->ycp_edit += fp->track[tLEFT] + fp->track[tTOP] * fpip->max_w;
		fpip->ycp_temp += fp->track[tLEFT] + fp->track[tTOP] * fpip->max_w;
		fpip->w        -= fp->track[tLEFT] + fp->track[tRIGHT];
		fpip->h        -= fp->track[tTOP]  + fp->track[tBTM];
	}

	if(fp->check[cDISP]){	// 適用範囲表示
		NegaPosi(fp,fpip);	// NegativePositive
		defpic.Store(fpip);
		return TRUE;
	}


	int i;
	short *bump = new short[fpip->w*fpip->h];
	short *bum  = bump;

	// 輪郭抽出
	Bump(bum,fpip->ycp_edit,fpip,fp->track[tBUMP]*16);

	// ぼかし用作業領域
	int   rowsize = (fpip->w+15)&~15;
	short *buf = new short[rowsize * kernel +16];	// 省メモリにし忘れてた
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
*	Bump	輪郭抽出
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
*	Blur	輪郭ぼかし
*
* 2行下まで横にぼかしたら縦にぼかせる。
* ぼかし用バッファは5行分。使いまわす。
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
*	BlurRow	横ぼかし
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
*	BlurCol	縦ぼかし
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
*	NegaPosi	ネガポジ
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
*	CopyEdge	範囲の外をコピー
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
*	設定ウィンドウプロシージャ
*===================================================================*/
BOOL func_WndProc( HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp )
{
	switch(message){
		case WM_KEYUP:	// メインウィンドウへ送る
		case WM_KEYDOWN:
		case WM_MOUSEWHEEL:
			SendMessage(GetWindow(hwnd, GW_OWNER), message, wparam, lparam);
			break;
	}

	return FALSE;
}


