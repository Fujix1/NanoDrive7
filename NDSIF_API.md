# NDSIF 汎用API仕様

文書版: 0.39 / Draft
更新日: 2026-10-15
対象: 音源レジスタ通信のホスト・受信機
策定対象: 正式v1、未成立。wire版の候補値: `01`

本書は製品・MCU・クロック生成IC・USBドライバー・搭載スロットに依存しないv1草案を定める。音源書き込みの要求IDはVGM準拠とし、全レジスタ書き込みを無応答BURSTに統一する。b版のフレーム・ID・実装記録は [互換資料](NDSIF_legacy/README.md) に分離する。個別ファームウェア・codecの対応状況は、その実装の資料で示す。

## 1. フレーム

送受信は双方向byteストリーム。整数はlittle-endian。トランスポートのread/write境界とフレーム境界は一致しない。

```text
00 + COBS(raw) + 00

raw = magic[2] + version[1] + message_type[1] + command_id[1]
    + transaction_id[2] + payload_length[2] + payload + crc16[2]
```

| raw offset | 型 | 内容 |
| ---: | --- | --- |
| 0 | 2 byte | ASCII `ND` = `4E 44` |
| 2 | u8 | wire版の候補値 `01` |
| 3 | u8 | メッセージ種別、下表 |
| 4 | u8 | 要求ID `command_id`、8bit全域を使用 |
| 5 | u16 | 応答照合用の要求番号 `transaction_id` |
| 7 | u16 | payload長、最大256 byte |
| 9 | byte列 | payload |
| 9 + payload長 | u16 | CRC-16/CCITT-FALSE |

| message_type | 意味 |
| --- | --- |
| `00` | 無応答要求。全レジスタ書き込みはこれを使う |
| `01` | 応答を要求する制御要求。レジスタ書き込みには使用不可 |
| `02` | 制御要求への応答 |
| その他 | 未定義。破棄 |

応答は `message_type=02` とし、要求のcommand_id・transaction_idをそのまま返す。command_id OR `80` は使わない。`A0` / `A5` / `D0` などbit7が立つ音源要求IDも、message_typeで要求と応答を区別する。

CRCの対象はmagic先頭からpayload末尾まで。poly=`1021`、init=`FFFF`、refin/refout=false、xorout=`0000`、チェック値 `123456789` → `29B1`。raw最大267 byte、COBS部最大269 byte、区切り込み最大271 byte。

不正COBS・CRC・magic・版・長さは無応答で破棄する。未完フレームは最後のbyteから500msで期限切れとなり、次の `00` まで破棄する。空の区切りは無視する。要求番号は重複排除や再送保証を提供しない。同じ書き込みを再送すると再実行され得る。

NDSIFは現在b版であり、本書は正式v1の策定草案。b版もwire版 `01` を使用するが、本草案との互換性を意味しない。wire版の値だけでは両形式を識別できないため、接続設定等でプロファイルを明示する。相手の形式を確認せず書き込み要求を送り、結果から自動判定する手順は定義しない。

## 2. 制御要求とレジスタ要求

`00`～`2F` はNDSIFの制御・ストリーム用領域、`30`～`FF` はVGM音源要求のための領域とする。VGMの同じ番号が存在しても、待ち・ファイル終端・データブロック等は音源レジスタ要求として定義しない。

| ID（hex） | 名称 | message_type | payload |
| --- | --- | --- | --- |
| `00` | RESET | `01` | 空 |
| `01` | PING | `01` | 0～32 byte |
| `02` | GET_INFO | `01` | 空 |
| `03` | SET_OUTPUT_VOLUME、任意機能 | `00` / `01` | attenuation:u8 |
| `04` | SET_CHIP_CLOCK | `00` / `01` | write_id:u8 + instance:u8 + hz:u32 |
| `05` | GET_CAPABILITIES | `01` | offset:u16 |
| `10` | AUDIO_DATA、任意プロファイル | `00` | 第8節 |
| `11` | AUDIO_EVENT、任意プロファイル | `00` | 第8節 |
| `12` | AUDIO_START、任意プロファイル | `00` | 第8節 |
| `13` | AUDIO_STATUS、任意プロファイル | `01` | 第8節 |
| VGMの音源要求ID | WRITE_* | `00`のみ | instance:u8 + page:u8 + register_pairs |

応答付き制御のpayload先頭はstatus:u8。`00` はソフトウェア処理完了、`01` は要求・引数・機能を拒否したことを示す。拒否応答はstatusだけを返す。完了は物理I/Oの読戻しや発音完了を保証しない。

**レジスタ書き込みにはstatus・ACK・完了応答を返さない。** 長さ・個体・ページ・対象が不正または未対応でも、書き込みへの応答は生成しない。message_type=`01` を付けたレジスタ要求も、書き込み・応答を行わず破棄する。受理確認が必要なホストは対応情報を先に調べ、初期化列の後続PINGや、対応する診断を利用する。PING成功は不正な先行書き込みが成功したという意味ではない。

無応答の未定義要求は破棄する。応答付きの制御領域の未定義要求にはstatus=`01` を返せる。応答フレームへ応答しない。ホストは制御応答をopcodeのbit7ではなく、種別・ID・要求番号で照合する。

## 3. VGM準拠の音源要求ID

以下は [VGM v1.71仕様の音源コマンドとdual-chip規則](https://github.com/vgmrips/vgmplay-legacy/blob/master/VGMPlay/vgmspec171.txt) に基づく。要求IDは16進、instanceとpageは数値。

| 要求ID | 名称・論理音源 | instance | page |
| --- | --- | --- | --- |
| `51` | WRITE_YM2413 / OPLL | 対応する個体 | 0 |
| `54` | WRITE_YM2151 / OPM | 対応する個体 | 0 |
| `55` | WRITE_YM2203 / OPN | 対応する個体 | 0 |
| `A5` | WRITE_YM2203_SECOND / OPNの2個目、VGM別ID | 1のみ | 0 |
| `5A` | WRITE_YM3812 / OPL2 | 対応する個体 | 0 |
| `5B` | WRITE_YM3526 / OPL1 | 対応する個体 | 0 |
| `5E` | WRITE_YMF262_PAGE0 / OPL3 | 対応する個体 | 0 |
| `5F` | WRITE_YMF262_PAGE1 / OPL3 | 対応する個体 | 1 |
| `A0` | WRITE_AY8910 / AY-3-8910 | 0=1個目、1=2個目等 | 0 |
| `D0` | WRITE_YMF278B、将来の対応対象 | 対応する個体 | 0・1・2 |
| `B7` | WRITE_OKIM6258、任意のレジスタ／ストリームプロファイル | 対応する個体 | 0 |

基本IDは論理音源を選び、明示instanceで個体を選ぶ。例: `55` / instance=1 と `A5` / instance=1 は同じYM2203の2個目を指す。VGMの2個目専用の別IDを扱う受信機は、そのIDとinstance=1の一致を要求する。`A1` / `A4` / `AA` / `AB` / `AE` / `AF` もそれぞれ元のYM系音源の2個目として予約する。実際に使えるID・個体・ページは第7節で確認する。

AYの2個目は `A0` / instance=1 で指定し、VGMの個体選択bit7をaddressへ含めない。VGMの `D0` 等の個体選択bitもpageへ含めない。取り込み側がそれぞれinstanceへ分離する。

`56` はVGMのYM2608ページ0、`57` はYM2608ページ1として予約する。この草案の追加対応対象には含めず、YM2151のBURST・クロック設定・その他の制御へ転用しない。`58` / `59` もVGMのYM2610の用途へ予約する。b版の同じ数値とは別の意味。

次の音源も将来の対応用に予約し、別音源・制御へ転用しない。予約は実装済み・使用可能を意味しない。

| 予約ID | 論理音源・用途 | 個体・ページ |
| --- | --- | --- |
| `52` / `53` | YM2612 / YM3438、ページ0 / 1 | 基本ID、明示instance。ページはそれぞれ0 / 1 |
| `A2` / `A3` | YM2612 / YM3438の2個目、ページ0 / 1 | instance=1。ページはそれぞれ0 / 1 |
| `50` | SN76489の1個目の基本ID | instance=0。将来の明示instance形式は別途策定 |
| `30` | SN76489の2個目、VGM別ID | instance=1 |

YM2612とYM3438はVGMと同じ要求IDを共有し、別音源IDを新設しない。品種の指定・対応情報は今後策定し、クロックHzに品種フラグを混ぜない。SN76489はaddress/value組ではなくラッチ／データbyteを書き込む音源のため、予約段階では第4節のレジスタpayloadを適用せず、byte列のBURST形式を今後策定する。その場合も書き込みのstatus・ACKは返さない。

## 4. レジスタ書き込み — 全てBURST

音源の要求IDをフレームのcommand_idに置き、payloadは次の共通形式にする。

```text
instance:u8 + page:u8 + (address:u8 + value:u8)[1..127]
```

payload長は4～256 byteの偶数。一つのフレームは一つの音源・個体・ページを対象にする。組の並び順を維持して処理する。別ページ・別個体は別フレームへ分ける。

pageはu8で0～255を表現する。単一ページの音源は0、2ページの音源は0・1、3ページの音源は0・1・2を区別する。fieldの表現範囲と、その音源が持つ有効ページは別。受信機は未対応ページを無音源のページ0へ読み替えない。

アドレスページは音源固有の空間で、ページごとの用途を規格共通の固定機能にしない。YMF262は `5E` とpage=0、`5F` とpage=1を一致させる。YMF278B等の明示ページ型IDは一つのIDのpayloadでページ0・1・2を選べる。AYのaddress範囲は `00`～`0F`。

YMF278Bの3ページは、[ymfmの音源実装](https://github.com/aaronsgiles/ymfm/blob/main/src/ymfm_opl.cpp)でも通常FM・上位FM・PCMのアドレス空間として区別される。NDSIFのpageはこの種の違いを保持するフィールドであり、YMF262の2バンクに制限しない。

対象・instance・page・長さ・音源のアドレス範囲をフレーム全体で検証してから書き込む。引数不正による部分書き込みを行わない。バーストは複数フレームの原子性、I/Oの成功、重複排除、再送を保証しない。単独の1組もBURSTと同じ無応答動作。

| 操作 | command_id | payload（hex） |
| --- | --- | --- |
| YM2151 register `20` ← `C7`、続けて `08` ← `78` | `54` | `00 00 20 C7 08 78` |
| YM2203の1個目、register `28` ← `F0` | `55` | `00 00 28 F0` |
| YM2203の2個目、register `28` ← `F0` | `A5` | `01 00 28 F0` |
| YM2413 register `20` ← `15` | `51` | `00 00 20 15` |
| OPL1 register `B0` ← `20` | `5B` | `00 00 B0 20` |
| OPL2 register `B0` ← `20` | `5A` | `00 00 B0 20` |
| OPL3 page1のregister `05` ← `01` | `5F` | `00 01 05 01` |
| AYの1個目、register `08` ← `0F` | `A0` | `00 00 08 0F` |
| AYの2個目、register `08` ← `0F` | `A0` | `01 00 08 0F` |
| ページ2型の例: YMF278B page2のregister `08` ← `01` | `D0` | `00 02 08 01` |


## 5. 初期化・情報・同期

RESET (`00`) は受信機が管理する音源・ストリームの状態を初期化する。ソフトウェアの位置・キュー・faultをクリアし、プロファイルの初期状態へ揃える。物理リセット、ミュート、準備用無音、安定待ちの具体的な方式・時間は受信機の実装事項。曲データを消費する前に必要な準備を終え、完了後にstatusを返す。

PING (`01`) は0～32 byteを受け取り、status=`00`と同じbyte列を返す。先行する即時要求を受信順に処理した後に応答するが、書き込みの個別成功や予約済みイベントの実行完了を示さない。

GET_INFO (`02`) の成功payloadは `status:u8 + name_length:u8 + name[bytes] + firmware_length:u8 + firmware[bytes]`。文字列はUTF-8、NULなし、全体は256 byte以内。製品名・FWの特定の文字列を必須にせず、機能対応の判定は第7節を使う。

SET_OUTPUT_VOLUME (`03`) は任意機能。attenuationは0～96、0=減衰なし、1～95=dB指定、96=ミュート。受信機が最終出力へ適用し、対応しない場合は応答付き要求で拒否する。物理的な音量素子・チャンネル数・制御バスは固定しない。

## 6. SET_CHIP_CLOCK (`04`)

```text
write_id:u8 + instance:u8 + hz:u32
```

write_idはVGM準拠の論理音源要求ID、hzはその音源本来の入力クロックHz。個体を明示し、VGMヘッダーのdual-chip等のフラグは含めない。ページごとの別IDは同じ音源・個体のクロックを指す。例えば `5E` / `5F` は同じYMF262のクロックで、ページ別のクロックではない。

受信側が搭載構成・互換音源・分周・クロック生成手段に応じて正規化する。ホストは代替チップのIDや変換済みHzへ置き換えない。

| 論理音源 → 物理音源 | 基本倍率・モード |
| --- | --- |
| 同種の音源 → 同種の音源 | 1倍、受信側で実現可能な値へ正規化 |
| OPL1 → OPL2 | 1倍、対応可能な機能範囲 |
| OPL1 / OPL2 → OPL3 | 4倍、OPL互換モード |
| OPL3 → OPL3 | 1倍、OPL3本来のHz |
| AY → YM2203の標準SSG構成 | 2倍 |

OPL1の3,579,545Hzを物理OPL3で鳴らす場合、ホストはwrite_id=`5B` / hz=3,579,545を送り、受信側が14,318,180Hzへ正規化する。OPL3本来の指定はwrite_id=`5E` / hz=14,318,180とし、再び4倍しない。AYの2MHzを標準のYM2203 SSG部へ割り当てる場合は物理4MHz。

未対応ID・個体、Hz=0、互換割り当てなし、実現できない設定、倍率計算のu32オーバーフローは適用しない。要求がmessage_type=`01`なら拒否を返し、`00`なら無応答。任意Hzの厳密な実現やクロック読戻しは保証しない。共有する物理音源・クロックには最後の設定が効くため、受信機は共有の制約を明示する。

## 7. GET_CAPABILITIES (`05`)（問い合わせ形式の草案）

要求payloadはoffset:u16。最初は0。成功応答は次の形式。

```text
status:u8 + max_payload:u16 + data_capacity:u32 + event_capacity:u16
 + control_features:u16 + next_offset:u16 + entry_count:u8
 + entries[entry_count]

entry = write_id:u8 + instance:u8 + page:u8 + features:u8
      + clock_mode:u8 + fixed_clock_hz:u32
```

固定部分14 byte、1entryは9 byte、256 byte上限では最大26entry。next_offset=`FFFF` は一覧の終端、それ以外は次に問い合わせるoffset。受信機は同じ対応構成の一貫した一覧を返す。ホストは終端まで取得し、循環・重複・不正長を拒否する。対応構成が変わった場合は取得をやり直す。

| features bit | 意味 |
| ---: | --- |
| 0 | 即時レジスタBURST |
| 1 | 位置付きレジスタイベント |
| 2 | 論理クロック設定 |
| 3 | この音源・個体の任意ストリームプロファイル |

各entryは論理音源本来の入力クロックについて、次の情報を返す。u32はlittle-endian。

| clock_mode | 意味 | fixed_clock_hz |
| ---: | --- | --- |
| `00` | 不明・未開示 | 0 |
| `01` | 固定クロック | 実際の固定入力周波数Hz、1以上 |
| `02` | NDSIFから変更可能（variable） | 0 |

例: YM2151が固定3,579,545Hzなら`54 00 00 03 01 99 9E 36 00`。
変更可能なYM2151なら`54 00 00 07 02 00 00 00 00`。
clock_mode=01のentryはfeatures bit2を立てず、02のentryはbit2を立てる。
不明を固定と読み替えず、固定以外のfixed_clock_hzは0とする。未定義のmodeや矛盾したentryは拒否する。
同じwrite_id・instanceの各pageは同じclock_mode・fixed_clock_hzを返す。

variableはSi5351等の特定ICを意味せず、NDSIF経由で設定を変更できることを示す。
任意Hz、連続可変、厳密な周波数精度を保証せず、対応Hz・正規化・共有制約は受信機資料で確認する。
fixed_clock_hzは現在値の読戻しではなく固定ハードウェアの入力周波数であり、ホストによる互換倍率の適用前の論理Hzで通知する。
固定ハードではホストはSET_CHIP_CLOCKを送らず、曲の要求Hzとの不一致を表示して利用を中止するか、明示的に対応した上流の変換処理を使う。
ストリームのSTART/kind=1でも、受信機は固定ハードで実現不能な周波数変更を適用しない。

この9 byteエントリーはrevision 39から使用する。revision 38の4 byte形式は残さず、同じGET_CAPABILITIES要求を新形式へ置き換える。
wire版の候補値01とAPI文字列1.0は変更しない。ホスト・受信機を同時に更新する。

control_features bit0はSET_OUTPUT_VOLUMEの対応を示す。他のbitは予約。max_payloadはこの版では256以下。data_capacityはストリームの未供給byte容量、event_capacityは未実行のレジスタ組等の件数。対応しない機能の容量は0。複数ストリームの個別容量が異なる場合の詳細通知は今後の拡張事項で、共通値は全対応ストリームで使用できる保守的な上限とする。

ページ0・1・2を持つ音源は、同じwrite_id・instanceについて各pageを別entryで示す。別IDにも対応する場合はその別IDも示す。この一覧は物理スロットを固定しない。変更可能な周波数の一覧・許容近似・共有関係の詳細な表現形式は別途定義する。

## 8. 任意ストリーム — MSM6258互換ADPCMプロファイル

以下は、レジスタ通信とは分離した任意プロファイル。全受信機にMSM6258・ADPCM・データ要求割り込みを要求するものではない。GET_CAPABILITIESで対応を確認して使う。別codec・一般PCMの形式はこの草案では定義しない。

stream_idにはVGMのMSM6258 ID=`B7`、instanceには個体番号を使う。各ストリームは独立した位置・codec state・faultを持つ。positionはRESETからのADPCM byte位置u32。1 byteはlow nibble→high nibbleの2samples。自然周回を認めない。

### 8.1. AUDIO_DATA (`10`)

```text
stream_id:u8 + instance:u8 + position:u32 + adpcm[1..250]
```

payloadは7～256 byte。positionは受信機のacceptedと完全一致させる。重複・欠落・逆順・END後・位置オーバーフローはfault。容量を超えるチャンクは部分受理せず拒否する。応答は返さず、状態はAUDIO_STATUSで確認する。

ホストは出力エンコーダの予測値・stepをチャンク・ノート・無音をまたいで継続する。無音も同じエンコーダで生成する。送信完了は受理・供給・実変換の完了と区別する。同じ音源への直接データレジスタ書き込みを、連続ストリームへ混ぜない。

### 8.2. AUDIO_EVENT (`11`)

```text
stream_id:u8 + instance:u8 + position:u32 + kind:u8 + kind_data
```

| kind | kind_data | 全payload長 |
| ---: | --- | ---: |
| 0 | write_id:u8 + target_instance:u8 + page:u8 + register_pairs[1..123] | 12～256、偶数 |
| 1 | hz:u32 + divider:u16、このストリームのクロック・分周 | 13 |
| 2 | pan:u8、このストリームの出力 | 8 |
| 3 | zero_pair:u8、END | 8 |

登録は同値を許すposition昇順、同値では受信順を維持する。レジスタ1組をキューの1件と数える。引数や容量を先に検証し、イベントの部分登録をしない。kind=0は全音源について論理要求ID・個体・ページを保持し、書き込み自体のACKやstatusを返さない。対象や位置の拒否はストリームfaultとして状態問い合わせに記録する。

kind=1のHzは4,000,000 / 8,000,000、dividerは512 / 768 / 1024。ホストのsample rate・リサンプラ・byte時間も同じ境界で変更する。kind=2は0=左右、1=左、2=右、3=オフ。一般音源のクロック予約はここへ流用せず、別仕様とする。

ENDは最後のDATA後、position=acceptedに置く。ホストは末尾をゼロPCMへ収束させ、エンコーダ状態に合う `80` / `08` を指定する。受信機は終了後もstateに合うゼロペアを反復できる。以後のDATA/EVENT・同一ストリームの再STARTは禁止し、再開はRESET後に行う。通常の離鍵・待機にENDを使わない。

### 8.3. AUDIO_START (`12`)

```text
stream_id:u8 + instance:u8 + initial_hz:u32 + divider:u16
```

payloadは8 byte。faultなし、未開始、accepted>0、played=0で開始する。初期Hz・dividerはkind=1と同じ。無応答で、開始成功はSTATUSのrunningで確認する。

受信機は必要な初期化・デコード開始・安定待ちをミュート中に終え、音楽データを消費する前に出力を準備する。具体的な待ち時間やI/O順序は受信機に依存し、規格で特定の250ms等へ固定しない。

### 8.4. AUDIO_STATUS (`13`)

要求payloadは `stream_id:u8 + instance:u8`。成功応答はstatus=`00` + u32×10で41 byte。

| u32の順序 | 名称 | 意味 |
| ---: | --- | --- |
| 0 | accepted | 受理済み末尾位置 |
| 1 | played | 音源へ供給済み末尾位置 |
| 2 | pending | accepted−played |
| 3 | underflows | データ不足回数 |
| 4 | overflows | データ／イベント容量超過回数 |
| 5 | rejected | ストリーム引数・位置の拒否回数 |
| 6 | max_pending | RESET後の最大pending |
| 7 | flags | 下表 |
| 8 | late_events | 適用位置を過ぎて処理したイベント件数 |
| 9 | max_event_lag | 最大イベント遅延、byte単位 |

flags bit0=running、bit1=ended、bit2=fault。原因bit3=transport reset、4=receive overflow、5=explicit discard、6=data underflow、7=data overflow、8=event overflow、9=invalid argument/position。未知の上位bitは保持し、faultはbit2で判断する。正常RESET後は全項目0。late_eventsだけではfaultとしない。

playedは供給済み数であり、DACの変換完了を保証しない。容量・実適用遅延・通信断時の音止めは受信機の実装と運用条件で確認する。プロファイルがfaultになったらデータを補って再開せず、ホストの古い生成・予約を止めてRESETから初期化する。

## 9. 予約と移行

VGM要求ID表の未対応音源は、そのVGM用途のまま予約する。任意の制御や別音源へ番号を再利用しない。16bitアドレス・16bit値・メモリー転送等は別のレジスタプロファイルとして今後定義し、8bit形式へ無理に詰め込まない。

b版の `54` は応答付き、`56` はYM2151バースト、`57`～`5B` は制御・ADPCMの用途だった。正式v1草案では `54` は無応答YM2151、`56` / `57` はYM2608へ予約、`5A` / `5B` はOPL2 / OPL1の無応答書き込み。これらは正式v1の策定に伴う変更案であり、成立済みのv1を改訂するものではない。個別実装の移行は旧形式との非互換を明示して行う。
