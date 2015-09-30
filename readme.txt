-----------------------------------------------------------------------
      WarpSharp for AviUtl              ver0.03
-----------------------------------------------------------------------

【機能】

  言わずと知れたAviSynthのWarpSharpフィルタの移植版です。

【インストール】

  warpsharp.auf を AviUtl の認識可能なフォルダにコピーしてください。

【使用方法】

  ○パラメータ

  cubicの値が10倍になっている他はAviSynth版とほぼ共通です。

  ・depth スライダ
        適用する強度

  ・blur スライダ
        輪郭をぼかす回数

  ・bump スライダ
        輪郭抽出時の閾値

  ・cubic スライダ
        ３次元補間係数

  ・上下左右 スライダ
        適用範囲の指定

  ・範囲表示 チェック
        適用範囲を階調反転して表示

  depth･bumpの初期値はAviSynth版の初期値とあわせている為に
  かなり強めの設定になっています。各自で調節することをお勧めします。

【注意】

  AviUtl 0.98 以降専用
  このプログラムはフリーソフトウェアです。
  このプログラムによって損害を負った場合でも、作者は責任を負いません。

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA,
    or visit http://www.gnu.org/copyleft/gpl.html .


【謝辞】

  AviSynth版WarpSharpの作者様に感謝します。

【配布元】

  MakKi's SoftWare
  http://mksoft.hp.infoseek.co.jp/

【更新履歴】

  2008/06/28   ver 0.03   適応範囲パラメータの最大値を拡張
                          ver0.02のちょっと高速化が全然効いてなかったのを修正
                          メモリ使用の無駄を解消
                          指摘されたバグについて見直し
  2004/01/12   ver 0.02   適用範囲を指定する機能追加
                          ちょっと高速化
  2003/10/20   ver 0.01a  最下端にゴミが出るバグ修正
  2003/10/19   ver 0.01   公開


mailto:makki_d210@yahoo.co.jp