# NanoDrive 7 VGM Player

NanoDrive 7 is a VGM player with YM2203 x 2 + YMF262 OPL3 + YM2413, which plays back VGM format files. It supports YM2203, AY-3-8910, OPL, OPL2, OPL3, YM2413 and MSX audio (FM only).

Nano Drive 7 は、YM2203 x 2 + YMF262 OPL3 + YM2413 を搭載した VGM プレーヤーです。

<br>
<br>


## マニュアル / Manual (Japanese and English)
[ND7_rev1.pdf](https://github.com/user-attachments/files/22449756/ND7_rev1.pdf)
<br>
<br>
<br>
## 回路図 / Schematic
[schematic.pdf](https://github.com/user-attachments/files/22195744/schematic.pdf)
<br>
<br>
<br>
## ファームウェアのアップデート方法 / How to update the firmware.

[(https://nanodrive.netlify.app/](https://nanodrive.netlify.app/) よりブラウザ経由で更新できます。USB接続してボタンをクリックし、デバイスを選択すると自動で書き換えが行われます。
h1romas4 さん制作です。

自分でコンパイルする場合は以下の手順です：

1) [Visual Studio Code](https://code.visualstudio.com/) をインストールし、拡張機能 [Platform I/O](https://platformio.org/)を導入します。これでコンパイル環境が完成します。
2) このGitをクローンするかダウンロードして、VSCode で開きます。初回、必要なファイル類は自動でダウンロードされるので数分間待ちます。
3) NanoDrive7 本体を USB で接続します。VSCode の左側の一番下の欄に「→」ボタンがあるのでクリックするとコンパイルと書き換え始まります。または CTRL + ALT + U でもOKです。
<br>
<br>
<br>

## Credits and licenses

- Open Font Render by takkaO: FTL license
  https://github.com/takkaO/OpenFontRender

- LovyganGFX by lovyan: FreeBSD license
  https://github.com/lovyan03/LovyanGFX

- PNGdec by Larry Bank: Apache 2.0 license
  https://github.com/bitbank2/PNGdec

- BIZ UDPGothic: SIL Open Font License 1.1
  https://fonts.google.com/specimen/BIZ+UDPGothic/license

- Portions of this software are copyright © The FreeTypeProject (www.freetype.org). All rights reserved.
