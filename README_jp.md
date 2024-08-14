# JTAG2UPDI (Clone)

- [原典の READEME(en_US)](README_orig.md)
- [この文書の英語版(en_US)](README.md)
- [この文書の日本語版(ja_JP)](README_jp.md)

これは [ElTangas/jtag2updi](https://github.com/ElTangas/jtag2updi) ファームウェアの静的なフォークだ。コードの大部分は同じだが、大規模な変更を伴うため upstream 連携は行なっていない。

分岐点 : https://github.com/ElTangas/jtag2updi/tree/07be876105e0b9cfedf2723b0ac88780bcae50d8

両者の差分は `diff -uBw jtag2updi-master/source jtag2updi-main/src` で得られるはずだ。`JTAG2.cpp` は、ほとんど別物である。

## 変化したこと

- NVMCTRL version 0,2,3,4,5 に対応する。原典の対応は 0 と 2 までである。
  - tinyAVR-0/1/2 、megaAVR-0 、AVR_DA/DB/DD/EA/EB を支援する。
    - UPDI層 SIB（System Information Block）に対応。
  - AVR_DU（NVMCTRLv4）支援は、2023/12時点で暫定／実験的。
  - 主な動作確認済デバイス： ATtiny202 ATtiny412 ATtiny824 ATtiny1614 ATmega4809 AVR32DA32 AVR128DB32 AVR32DD14 AVR64DD32 AVR64EA32 (2023/11時点)
- AVRDUDE 7.3 対応。
  - 動作確認済： `AVRDUDE` `7.2`、`7.3`（2023/11時点で開発中）
  - `6.8`以前では特に修正された構成ファイルとセットで用いる。`7.0`以降は標準の構成ファイルで十分である。
  - `-D`オプションを用いて、部分メモリブロックだけを書き換えることができる。（ブートローダーがそうするように）
    - これによりインタラクティブモードの`write/elase <memtyp>`コマンドも使用可能になった。
  - 施錠済デバイス対応。
    - USERROW の盲目的書き込み。
    - LOCK_BITS 施錠／解錠。
  - メモリ範囲指定違反検出の強化。
    - AVR_DA/DB/DDの 256 word FLASH バルク読み/書きを許す。
    - AVR_DA/DB/DDの 1 word EEPROM 読み/書きを許す。（標準の構成ファイルは 1 byte 単位に制限）
    - AVR_EA/EBの 4 word EEPROM 読み/書きを許す。
  - `-b`オプションで `230400`、`460800`、`500000`が多くの環境で実用的に指定可能。
    - Arduino互換機はこの限りではない。大容量バルク読み書き時は逆にホスト側タイムアウト／エラーの制約を受けるので注意。（一般に通信速度を速くするほどUSBパケットロスト率は増加する）
  - いくつかの機能拡張。
    - 高電圧制御には対応していない。これには追加のハードウェアと専用の制御コードが必要だ。現在の UPDI対応デバイス用の HV制御には 2つの異なる仕様がある。両方を同時にサポートする手軽な方法は存在せず、それがこの問題を難しくしている。（UPDI4AVRの技術情報を参照せよ）
- ファームウェアビルド＆インストールは、Arduino IDE 1.8.x/2.x のみ検証されている。他の方法については定かではない。
  - Arduino IDE の仕様に合わせて`source`ディレクトリが`src`に変更された。`jtag2updi.ino`ファイルの配置場所も異なる。
  - サポート対象としない付属ファイルは削除された。必要なら原典からコピーされたい。

ファームウェアインストール可能機材：

- ATmegaX8シリーズ --- Arduino UNO Rev.3、Arduino Mini、Arduino Micro (ATmega328P)
- megaAVR-0シリーズ --- Aruduino UNO WiFi Rev.2 (ATmega4809)
- AVR_DA/DBシリーズ --- Microchip Curio City

> ファームウェアインストール後はオートリセット機能を無効化すべく、/RESETとGNDの間に 10uFコンデンサの追加装着を推奨する。

技術的要素の多くは [UPDI4AVR](https://github.com/askn37/multix-zinnia-updi4avr-firmware-builder/) を開発する過程から得られた知見に由来する。
UPDI4AVRは megaAVR-0以降のデバイスにしかインストールできないため、古い Arduino を有効活用する目的で、この JTAG2UPDI(Clone) は制作された。

## 技術情報

### NVMCTRL

NVMCTRLは各系統によって仕様が異なる。そのバージョンは SIBから読み取れる数値によって、version 0から5に分類される。それぞれは制御レジスタの位置や定義値が異なり、制御方法も異なるため互いに互換性を持たない。

各バージョンでの制御レジスタ定義の一覧を以下に示す。

|Offset|version 0|version 2|version 3|version 4|version 5|
|-|-|-|-|-|-|
|Series|(tiny,mega)AVR|AVR_DA/DB/DD|AVR_EA|AVR_DU|AVR_EB
|$00|CTRLA|CTRLA|CTRLA|CTRLA|CTRLA
|$01|CTRLB|CTRLB|CTRLB|CTRLB|CTRLB
|$02|STATUS|STATUS|-|CTRLC|CTRLC
|$03|INTCTRL|INTCTRL|-|-|-
|$04|INTFLAGS|INTFLAGS|INTCTRL|INTCTRL|INTCTRL
|$05|-|-|INTFLAGS|INTFLAGS|INTFLAGS
|$06|DATAL|DATAL|STATUS|STATUS|STATUS
|$07|DATAH|DATAH|-|-|-
|$08|ADDRL|ADDR0|DATAL|DATA0|DATAL
|$09|ADDRH|ADDR1|DATAH|DATA1|DATAH
|$0A|-|ADDR2|-|DATA2|-
|$0B|-|_ADDR3_|-|DATA3|-
|$0C|-|-|ADDR0|ADDR0|ADDR0
|$0D|-|-|ADDR1|ADDR1|ADDR1
|$0E|-|-|ADDR2|ADDR2|ADDR2
|$0F|-|-|_ADDR3_|_ADDR3_|_ADDR3_

> _斜体_ のシンボルは定義されているものの、実際には機能しない。常にゼロであるべきだ。\
> version 1 の実装は知られていない。

`NVMCTRL_ADDR2`の最上位ビットには特別な意味があり、0は一般データ領域を、1はFLASHコード領域（現在は最大128KiB）のアドレスを指定するフラグとして使われる。version 0 にはこれがなく、データもコードも同一の64KiB空間内で区別されない。

どのバージョンでも、FLASH メモリはワード指向である。ワードアライメントに従わない操作は、期待した結果を得られない。

NVM書き込みは、直接対象のアドレスに（UPDIのST/STS指令、あるいはアセンブリのST/SPM命令で）メモリデータを書く。するとその指定アドレスが ADDR レジスタに、メモリデータが DATA レジスタに反映され、内部転送処理に渡される仕組みだ。

#### NVMCTRL version 0

この系統のみ 16bitアドレスバス体系であり、コード空間もデータ空間と同じ64KiB以内にある。ただし実際のコード領域先頭アドレスのオフセットは、tinyAVR系統が 0x8000、megaAVR系統が 0x4000 から始まるため、両者は区別して扱わなければならない。

その他、この系統には以下の特徴がある。

- メモリ空間には現れない特別な作業用緩衝メモリを持ち、FLASHと EEPROMで共用する。
- FLASHも EEPROMも、緩衝メモリ範囲を超えない限り続けてメモリデータを書いて良い。
- 緩衝メモリを満たしたのちに、CTRLA に目的の指令コードを書く。制御器が活性化され、目的処理が終わると STATUS レジスタに処理結果が反映される。制御器は処理終了で不活性化する。
- EEPROMも FLASHと同等のページ粒度でバルク書き込みができるため、処理が高速。
- USERROW へは EEPROM と同じ指令コードを使用して書ける。（しかし専用に用意された方法を用いるべきだ）
- FUSEメモリは特別な専用書き込み指令がある。直接 DATA と ADDR レジスタに書いた後、専用指令を CTRLA に書く。
- AVR命令セットでは SPM命令が存在しない。未定義ではなく NOPに同等。一方で LPM命令はあるため、コード領域先頭アドレス相対で FLASH領域を読むことはできる。

#### NVMCTRL version 2

この系統は 作業用緩衝メモリ を持たない。1ワード分のバッファ（つまりDATAL/Hレジスタ）だけを持つ。従って処理手順が異なる。

- CTRLA にこれから行う目的の指令コードを書いて制御器を活性化させる。
- ST/SPM命令で目的のNVMアドレスにメモリデータを書く。
- STATUSレジスタを確認し、目的処理を終えたなら CTRLA に NOCMD（またはNOOP） 指令を書いて制御器を停止させる。
- FLASH領域は、ページ範囲を超えない限り続けてメモリデータを書いて良い。
- EEPROM領域は、連続して書けるのは 1ワード粒度（2バイト以内）に限られる。
- FUSEは、EEPROMと同じ指令コードを使用して書ける。
- USERROWは、FLASHと同じ指令コードを使用して書ける。（しかし専用に用意された方法を用いるべきだ）

この系統は、FLASHページ粒度が 512バイトである。しかしこの大きさに対応したNVMライターは一般的ではないことから、`AVRDUDE`は 256バイトの2個のブロックに分割して書き込もうとする。

#### NVMCTRL version 3,5

この系統は FLASHと EEPROMでは互いに異なる 作業用緩衝メモリ を持っている。したがってそのページ範囲内でなら、連続してメモリデータを書ける。

- CTRLA に NOCMD（またはNOOP）指令を書いて制御器の不活性化を確実にする。
- ST/SPM命令で目的のNVMアドレスにメモリデータを書く。
- FLASHも EEPROMも、それぞれの緩衝メモリ範囲（128バイトと8バイト）を超えない限り連続してメモリデータを書いて良い。
- 緩衝メモリを満たしたのちに、CTRLA に目的の指令コードを書く。制御器が活性化され、目的処理が終わると STATUS レジスタに処理結果が反映される。制御器は処理終了で不活性化する。
- FUSEは、EEPROMと同じ指令コードを使用して書ける。
- USERROWは、FLASHと同じ指令コードを使用して書ける。（しかし専用に用意された方法を用いるべきだ）
- （version 5に特有の）BOOTROWは、FLASHと同じ指令コードを使用する。

UPDIからの NVM書き換え作業では CTRLC を使わないため、version 3 と 5 では同じ制御処理を共用できる。ただしNVM領域の実体アドレスの多くが異なるため、取り扱いは区別される。

#### NVMCTRL version 4

この系統は version 5 の NVMCTRLレジスタファイルと、version 2 の NVMCTRL指令セットを持つ。AVR_DUに実装されているが（おそらく）使用方法は version 2 に準じている。ただしNVM領域の実体アドレスの多くが異なるため、取り扱いは区別される。

この系統は 作業用緩衝メモリ を持たない。DATAレジスタは 2ワード分があるため（おそらく）EEPROMは 4バイト単位で書ける。

この系統は、FLASHページ粒度が 512バイトである。しかしこの大きさに対応したNVMライターは一般的ではないことから、`AVRDUDE`は 256バイトの2個のブロックに分割して書き込もうとする。

### 部分メモリブロック書き込み

`AVRDUDE`の`-D`オプションとインタラクティブモードの`write/erase <memtype>`コマンドを使用可能とするには、各メモリ領域のページ消去に対応していなければならない。原典の JTAG2UPDI はこれらを処理できなかった。実際にすべきことは一般的なブートローダーがしていることと同じで、指定されたアドレスのページをまず消去し、それから与えられたデータをそのメモリに書くことである。

> JTAGICE mkII 本来の、XMEGAメモリページ消去命令は`AVRDUDE`では使用されない。代わりに通常のメモリ読み／書き命令で代用される。

この時、512バイトページの場合は`AVRDUDE`が複数のメモリブロックに分割してデータを送信する仕様のため、その先頭アドレスに応じてページ消去の可否を判定する配慮が必要になる。同様の理由で、実デバイスのページ粒度と異なる設定が`AVRDUDE`構成ファイルに書かれていると部分メモリブロック書き込みは（緩衝バッファ内容が不正になり）正しい処理結果を示さない。

### 施錠済デバイス対応

「デバイスの施錠」とは主にセキュリティ上の要請からデバイスに実装されている機能の一つだ。施錠することによってデバイス内に記憶されているフラッシュメモリやEEPROMの内容の不正な改竄や読み出しを阻止する。ただしUPDIへのアクセスは拒否されず、チップ全体消去と、USERROW領域への盲目的書き込みは許可される。

デバイスを施錠するには、LOCK_BITS領域を既定値以外に書き換えるだけで良い。

```sh
# デバイス施錠操作
$ avrdude ... -U lock:w:0:m
```

施錠されたデバイスへは、USERROW領域への書き込みだけが許される。読み出しは許されないので読み戻し検証はできない。つまり`-F -V -U`の三つ組みオプションが少なくとも必要だ。本当に書き込みが成功したか否かはデバイスに事前に書き込まれたアプリケーションだけが知ることができる。

一方で、施錠されたデバイスからはデバイス署名を読み出せない。`-F`オプションがない場合はそれ以後の操作には進めなくなる。

なお JTAG2UPDI (Clone) の場合、施錠デバイスからは偽装署名（`ENABLE_PSEUDO_SIGNATURE`有効時）を返すので、例えば施錠された ATmega4809 の場合なら次のようなヒント（probably）が得られる。

```plain
$ avrdude ... -FVU userrow:w:12,34,56,78:m

avrdude warning: bad response to enter progmode command: RSP_ILLEGAL_MCU_STATE
avrdude warning: bad response to enter progmode command: RSP_ILLEGAL_MCU_STATE
avrdude: AVR device initialized and ready to accept instructions
avrdude: device signature = 0x1e6d30 (probably megaAVR-0 locked device)
avrdude warning: expected signature for ATmega4809 is 1E 96 51

avrdude: processing -U userrow:w:12,34,56,78:m
avrdude: reading input file 12,34,56,78 for userrow/usersig
         with 4 bytes in 1 section within [0, 3]
         using 1 page and 60 pad bytes
avrdude: writing 4 bytes userrow/usersig ...
Writing | ################################################## | 100% 0.01 s
avrdude: 4 bytes of userrow/usersig written

avrdude done.  Thank you.
```

施錠されたデバイスを開錠するには`-e -F`オプションを用いてチップ全体消去を強制する。施錠鍵は初期値に戻るが、一度電源を切らないと（FUSE領域なので）状態が正しく反映されないかもしれない。開錠とフラッシュ復元を同時に行う場合は、`-U`で正しい施錠鍵を最初に書き込むのが良さそうだ。

```sh
# デバイス解錠操作（チップ消去）
$ avrdude ... -eF
```

> [!TIP]
> FUSE設定で UPDI制御ピンを無効化した場合、JTAG2UPDI 実装ではそれを覆して復元することはできない。

> [!CAUTION]
> AVR-DU/EBファミリの場合、`FUSE_PDICFG`が既定値以外の時に施錠すると、以後絶対に UPDI操作ができなくなる。

### メモリ範囲指定違反検出の強化

これは特に、許されない書き込み／読み出しデータ量指定を拒否する機能である。拒否されると`AVRDUDE`は1バイト単位読み書き操作にフォールバックすることが多いため、これを JTAG2UPDI 側から意図的に制御する。普通、エラー原因は構成ファイルの編集ミスや、パーツ指定の誤りに由来する。

- 0バイト読み書きは許さない。これは意図的に設定しない限り生じないはずだ。（しかし不可能ではない）
- 真の（NVMCTRLバージョンで制限される）ページ粒度を超えるデータ長の EEPROM書き込みを許さない。
- FLASH領域への書き込みは、ページ粒度または特別な定義値に一致しなければ禁止される。

### CMND_GET_PARAMETER

`PAR_TARGET_SIGNATURE`問い合わせに対応している。これは可能ならば UPDI から SIB（System Information Block）を読み出して返す。SIB は一般的なメモリ読み出しではないため`CMND_GET_PARAMETER`を使用するのは JTAGICE mkII プロトコル規約に違反しない。返されるデータ長は`PDI/SPI`デバイス（それらでは2バイト固定だ）と違って 32バイトに及ぶ。

> `PP/HV/PDI/SPI`デバイスではデバイス署名が通常のメモリ読み出しでは読めないないことから`PAR_TARGET_SIGNATURE`問い合わせを用いる。XMEGA世代以降のデバイスではデバイス署名が通常のIOメモリ領域にあるためこれを使用せず、代わりに通常のメモリ読み出しで行われる。

### AVR EA/EBシリーズの取り扱い

潜在的に致命的な設計ミスが多く、非公開の（または未発見の）シリコンエラッタが多い。
NVM操作に関しては以下の点に注意されたい。

- CRCSCAN周辺機能に致命的なエラッタがある。FUSE_SYSCFG0 の変更でこれを有効化すべきではない。AVR EAでは最新の製造ロットで修正された。
- __AVR_EB__ で、UPDI周辺機能内のチップ消去機能が正常に動作しない。NVMCTRL周辺機能のチップ消去は正常に動作する。
- __AVR_EB__ で、LOCK.KEY と FUSE.PDICFG の動作が公開情報通りに動作しない。
チップ消去の件と合わせて LOCK.KEY 機能の設計ミスと推測される。
これらのヒューズ書き換えは容易にチップをブリックさせ、復旧する手段がない。

## ビルドオプション

これらのマクロ宣言は、`sys.h`に用意されている。

### NO_ACK_WRITE

既定で有効。UPDIのバルクデータ書き込みを許可する。無効にするとメモリ書き込み速度が半減するが、それしか効果が及ばない。

### DISABLE_HOST_TIMEOUT

既定で無効。クライアント側 JATG通信タイムアウトを制限しない。これはインタラクティブモードを多用する時に役立つ。しかしホスト側が キープアライブを実装しているなら有効にする必要はない。

> UPDI 通信速度を下げないと成功しないような場合は、配線負荷が過大である。普通は過剰なシリーズ抵抗やプルアップ抵抗を除去し、配線を十分短くすれば改善する。

### DISABLE_TARGET_TIMEOUT

既定で無効。UDPI通信タイムアウトを制限しない。副作用が大きいため有効にすべきではない。

### INCLUDE_EXTRA_INFO_JTAG

既定で無効。`SET_DEVICE_DESCRIPTPOR`パケットの応答に、獲得された詳細なデバイス情報を付加する。`AVRDUDE`を`-vvvv`付きで実行すれば表示されるが、ヒューマンリーダブルではない。

### ENABLE_PSEUDO_SIGNATURE

既定で有効。UDPIが無効化されておらず、かつデバイスが施錠されているならば擬似的なデバイス署名を返却する。現在生成される擬似署名は以下の 6種。

|Signature|Description|
|-|-|
|0x1e 0x74 0x30|施錠されたtinyAVR-0/1/2
|0x1e 0x6d 0x30|施錠されたmegaAVR-0
|0x1e 0x41 0x32|施錠されたAVR_DA/DB/DD
|0x1e 0x41 0x33|施錠されたAVR_EA
|0x1e 0x41 0x34|施錠されたAVR_DU
|0x1e 0x41 0x35|施錠されたAVR_EB

これらに対応するパーツ設定を構成ファイルに追加すると、その違いを容易に可視化できるだろう。同時にこれは施錠デバイスから唯一取得可能な SIBの要約でもあり、HV制御せずとも解錠可能なデバイスであることをも明らかにする。

```sh
#------------------------------------------------------------
# Locked device pseudo signature
#------------------------------------------------------------

part
    id        = "tinyAVR-0/1/2 locked device";
    signature = 0x1e 0x74 0x30;
;

part
    id        = "megaAVR-0 locked device";
    signature = 0x1e 0x6d 0x30;
;

part
    id        = "AVR-DA/DB/DD locked device";
    signature = 0x1e 0x41 0x32;
;

part
    id        = "AVR-EA locked device";
    signature = 0x1e 0x41 0x33;
;

part
    id        = "AVR-DU locked device";
    signature = 0x1e 0x41 0x34;
;

part
    id        = "AVR-EB locked device";
    signature = 0x1e 0x41 0x35;
;
```

この機能を無効化した場合のデバイス署名は、常に`0xff 0xff 0xff`で応答する。

## 更新履歴

- 2024/08/15 AVR_DU対応の修正（AVRDUDE 8.0準拠）
- 2024/01/26 NVM/V0 USERROW 対応の修正
- 2024/01/08 AVR_EB用のチップ消去と注意事項の追記
- 2023/12/19 AVR_DU用の暫定／実験的対応を追加（AVRDUDE 7.4のために）
- 2023/11/28 最初の版

## Copyright and Contact

Twitter(X): [@askn37](https://twitter.com/askn37) \
BlueSky Social: [@multix.jp](https://bsky.app/profile/multix.jp) \
GitHub: [https://github.com/askn37/](https://github.com/askn37/) \
Product: [https://askn37.github.io/](https://askn37.github.io/)

Copyright (c) 2024 askn (K.Sato) multix.jp \
Released under the MIT license \
[https://opensource.org/licenses/mit-license.php](https://opensource.org/licenses/mit-license.php) \
[https://www.oshwa.org/](https://www.oshwa.org/)
