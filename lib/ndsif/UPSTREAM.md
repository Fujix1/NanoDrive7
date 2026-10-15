# ND8 からの取り込み記録

取り込み日: 2026-10-15。参照先: `NanoDrive8-1` ワークスペース。
参照 HEAD: `c0929182e193162ae73ff14405b0caed2b6149ec`。
API: `1.0` / revision `39`、正式 v1 策定草案。
以下のハッシュは取り込み時の実ファイルの内容を示す。

- `lib/ndsif/ndsif.h`: `23df1681534d8f5cbe16f2f1da8894f5030eaf28d4fc024f155087279ddc458c`
- `lib/ndsif/ndsif.cpp`: `44d4b61815c9e97580e3a3ac7eafd4f4d382548d08720dc0a2e6793a54d2ff66`
- `include/serialman.h`: `dd74aa5fb3a20a9ad8dd107155d1f694d627294ebd27caf2feb8714d98a0cc43`
- `src/serialman.cpp`: `76ccfd98da19f59d441a76a0f24f1718949a48130346aa5d51fb64461e48b182`

## 共通ライブラリの差分

`ndsif.h` / `ndsif.cpp` のフレーム形式、COBS、CRC、タイムアウト、制御・応答処理は ND8 を基準とする。ND8 の `configure()` と YM2151 callback は互換性のため維持した。

ND7 に必要な拡張は次の 2 点に集約する。

- `setRegisterWriter()`: 音源 ID・instance・page・レジスタ列をボードへ渡す。ライブラリはフレームと Capability の対応を検証し、ボードは全レジスタの検証後に書き込む。
- `setDeviceInfo()`: GET_INFO の製品名と GET_CAPABILITIES のストリーム／イベント容量を設定する。ND7 は両容量を 0 にする。

`configure()` はこの拡張状態も初期化する。製品名と FW 文字列は各 63 byte までとし、応答上限を守る。

## ボード側の差分

`src/serialman.cpp` の先頭の匿名 namespace に ND7 の Capability、リセット、クロック、音量、レジスタ列の処理を置く。USB/RX/TX のライフサイクルとバッファ処理は ND8 の構成を維持する。

ND8 固有の SerialAudio・OKIM6258・ADPCM・チャンネルマスク LED 処理は取り込まない。通信欠落時はレジスタ音源をリセットして再同期する。画面への更新要求は `serialModeUpdateFooter()` へ渡し、受信タスクでは描画しない。

今後 ND8 の NDSIF 更新を取り込む際は、共通ライブラリの上記拡張、ND7 のボード callback、登録した Capability の整合を確認し、`tests/run_native.py` と実機確認を行う。API が変更された場合は仕様書も同時に更新する。
