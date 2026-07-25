# LIFT-X — Riser MIDI Synthesizer (OTODESK)

ライザー専用MIDIシンセサイザー。JUCE 8.0系 / VST3 + Standalone。v0.4。

## v0.4.1 追加: FXのソース別ルーティング

FXタブの各エフェクトタブ (SAT / CHORUS / DELAY / REVERB / DUCK) に
**ROUTE: [OSC 1] [OSC 2] [OSC 3] [NOISE]** 行を追加。FILTERタブと同じ操作感で、
**OFFにしたソースはそのエフェクトを完全にバイパスして素通し**します。
5エフェクト × 4ソース = 20系統を個別に設定できます。

使用例:
- サブ担当のOSCだけリバーブを外し、低域を濁らせずに上物だけ広げる
- ノイズにだけディレイを掛けてスネアロール感を出す
- OSC1にはサチュレーション、OSC2にはコーラス、と役割を分ける

### 実装方式 (なぜソース毎に4系統持たないか)

FxChain 1系統のメモリは 96kHz時で **15.2MB** (ShimmerReverbが16ch×2秒で11.7MBを占める)。
ソース毎に独立したチェーンを持つと **61MB / 192kHzでは122MB**、CPUも4倍になり非現実的です。

そこで **エフェクトのモジュール実体は各1個のまま共有し、エンジン出力を
OSC1/2/3/Noiseの4バスに分離したままFXチェーンへ通す**方式を採りました。
各スロットでは「ルーティングされたバスの合計」をモジュールへ入力し、
**モジュールが加えた変化量(差分)を対象バスへ均等配分**して書き戻します。

- **全ソースON時のバス合計は、従来の単一信号処理と数値的に一致**します
  (誤差は浮動小数の丸めのみ。6種のスロット構成 × 12万サンプルで最大 3.6e-07 を確認)。
  **既存70プリセットの音は一切変わりません。**
- **Ducking** は入力に依存しない純ゲインなので、差分ではなくゲインを各バスへ
  直接適用します (下流ルーティングとの相性が良く、合計も厳密に一致)。
- **スロット間ソフトクリップ**も「合計」に対して判定し、求まったゲイン比を
  全バスへ配分するため、従来と同じ歪み方になります。
- **サチュレーションは合計に対して掛かります**。非線形処理は Σf(x) ≠ f(Σx) のため、
  ソース別に個別に歪ませると既存プリセットの音が変わってしまうためです。
- ルーティング対象が0本のときも、モジュールには無音を通して内部バッファ/LFOを
  進め続けます (再ルーティング時の陳腐化バースト防止)。

メモリ増加はブロックサイズ分のバス4本 (数十KB) のみ、CPU増加も加算処理の分だけです。

その他:
- モノラル出力時のダウンミックスをFXチェーンの**後**に移動。コーラス/リバーブを
  ステレオのまま処理してから畳むため、モノ和が正しくなりました。
- 20個のルーティングパラメーターはパラメーターリストの**末尾**に追加しています
  (既存セッションのオートメーション割り当てに影響しません。旧セッションでは
  全ON扱いになるため挙動も変わりません)。

## v0.4.0 追加/修正

### 追加: Pitch ENV スケールクオンタイズ

CONFIGタブ最上段に **PITCH ENV - SCALE** セクションを新設。

- **SCALE QUANTIZE (On/Off)** — OFFは従来通りカーブ通りの滑らかなピッチ変化。
  ONにするとPitch ENVがKey+Scaleの構成音へスナップし、階段状のライザーになる。
- **KEY** — C〜B の12種。
- **SCALE** — **全70種**。カテゴリ見出し付きのコンボボックスで選択する。
  - BASIC (16): Chromatic / Major / Natural Minor / Major・Minor Pentatonic /
    Dorian / Lydian / Mixolydian / Phrygian / Harmonic・Melodic Minor /
    Whole Tone / Octaves & Fifths / Quartal / Sus2-4 Cloud / Diminished 7th
  - MODES & VARIANTS (15): Locrian / Blues Minor・Major / Harmonic Major /
    Double Harmonic / Phrygian Dominant / Lydian Dominant・Augmented /
    Mixolydian b6 / Half Diminished / Altered / Dorian b2・#4 / Lydian #2 / Ultra Locrian
  - WORLD (9): Hungarian Minor・Major / Neapolitan Minor・Major / Enigmatic /
    Persian / Oriental / Nine-Tone / Spanish 8-Tone
  - INDIAN (4): Todi / Marva / Purvi / Ahir Bhairav
  - JAPAN / ASIA (10): Hirajoshi / In-Sen / Iwato / Kumoi / Yo(Ryo) /
    Miyako-Bushi(都節) / Ryukyu(琉球) / Chinese Jiao / Egyptian Pent / Balinese Pelog
  - SYMMETRIC / BEBOP (7): Prometheus / Tritone / Augmented /
    Half-Whole・Whole-Half Diminished / Bebop Dominant・Major
  - CHORD TONES (9): Major・Minor・Sus4 Triad / Maj7 / Min7 / Dom7 /
    Min9 / Maj9 / Octaves Only
- **APPLY TO (OSC 1 / 2 / 3)** — OSC毎に量子化の適用可否を切替。片方だけ
  ノンクオンタイズにして滑らかな層を重ねる、といった使い方ができる。
- 量子化はCOARSE加算の**前**に行う。OSC2を+12stすれば常に正確な1オクターブ上を保つ。
- 量子化中はピッチ平滑の時定数を τ≒4ms → **τ≒1.2ms** に切り替え、
  段差をはっきり出しつつクリックを回避する。
- **デフォルトはOFF**（従来動作）。SCALEの初期値は Natural Minor / KEY=C。

#### Start/End キーの自動スナップ

**KEY / SCALE / APPLY TO のいずれかを変更すると、対象OSCの Start Key と End Key が
自動的にスケールの最寄り構成音へ再計算されます。** スナップ後はMIDIラーンや
DAWオートメーションを含めユーザーが自由に変更でき、次にKey/Scaleを触るまで
再スナップされません。

- 対象は `APPLY TO` がONのOSCのみ。OFFのOSCのキーには一切触れない。
- 0〜127を超える場合はオクターブ単位で内側へ折り返す。
- **プリセット読込・セッション復元では発火しない** — プリセットが持つStart/End
  キーがそのまま尊重される (適用直後にスナップ済み状態として記録するため)。
- 全70スケール × 12キー × 全128ノートで、範囲内・構成音への着地・移動量6半音以内・
  冪等性 (二重適用で変化しない) を検証済み。

### 追加: PITCH RAIL (Pitch ENV ライブ表示)

Pitch ENVは「絶対音程」を扱うため、他パラメーターのようなノブ帯 (ModBand) 方式が
使えません。そこでMAINタブの各OSC列、ST/ENDボタンのすぐ上に専用バーを新設しました。

- **左端 = Start Key / 右端 = End Key** に正規化した軌道
  (上昇・下降どちらでも左→右が進行方向)
- 現在ピッチまでの塗りつぶし + **ライブドット**
- 中央に**現在の音名をリアルタイム表示** (例 `E3`)。
  非量子化時でセント誤差が5cent以上あれば `E3 +23` のように併記
- **Scaleクオンタイズ有効時は、Start〜End区間に含まれるスケール構成音の位置に
  目盛りを描画** — 「今どの音を踏んでいるか」「次にどこへ跳ぶか」が一目で分かる
- 発音していないときもENV評価位置に追従するため、LIFT MANUALでノブを動かすだけで
  ピッチを確認できる
- 更新は VBlank同期 (画面リフレッシュレート)。値が動いたときだけ再描画

### 修正: サンプルレート依存の解消 (44.1 / 48 / 88.2 / 96 / 176.4 / 192kHz)

- **コントロールティック平滑がSR依存だった問題** — LIFT/DETUNE/SPREAD/
  FILTER CUTOFF/RES/NOISE CUTOFF・RESの平滑係数が固定値 (0.3 / 0.35 / 0.5) で、
  ティック間隔 (32サンプル) が短くなる高SRでは実時間の時定数が最大**4.4倍速く**なり、
  44.1kHzと96/192kHzで音が変わっていた。実時間τから係数を算出する方式へ変更し、
  全SRで44.1kHz時と同一の挙動になった。
- **ノイズオシレーターのSR依存** — Pink (Paul Kellett) / Brown の係数は44.1kHz設計で、
  高SRではフィルターの折れ点が周波数軸上へ持ち上がり「明るいピンク/ブラウン」に
  なっていた。Whiteも帯域がNyquistまで広がり192kHzでは可聴帯域のパワーが
  約6dB低下していた。→ **ノイズ生成を44.1kHz固定クロックで行い線形補間で
  ホストSRへ伸ばす**方式に変更。全SRでスペクトルとレベルが一致する
  (44.1kHz時は補間係数1.0で従来と同一出力)。
- FILTER / NOISE CUTOFF のターゲット値を 20Hz〜0.45×SR にクランプ。

### 修正: 安定性・堅牢性

- **モノラル出力のダウンミックス係数バグ** — `L + 0.5×R` になっており、
  モノ環境で約1.5倍の音量かつ左右バランスが崩れていた (UNISON SPREAD時に顕著)。
  `0.5×L + 0.5×R` へ修正。
- **DAW再起動でプリセット名が失われる問題** — プリセット名がステートに保存されて
  おらず、復元後は常に "Init" 表示になっていた。ステートへ保存/復元するよう修正。
- **ブロックサイズ超過での無音** — ホストが `prepareToPlay` の申告値より大きい
  ブロックを渡すと (オフラインバウンス/フリーズ等) 無音+リセットになっていた。
  申告値の2倍または8192サンプルの大きい方を事前確保して吸収する。
- **ノートオン時の平滑スナップ漏れ** — FILTER CUTOFF/RES、NOISE CUTOFF/RES、
  DETUNE/SPREAD がスナップ対象外で、前ノート終端の値からグライドしていた。
  発音頭の意図しないスイープを解消。`hardReset()` の初期化漏れ (cutSm/noiseCutSm) も修正。
- **プリセット◀▶ナビゲーション** — Factory/Userで同名プリセットがあると
  位置を見失っていた。読込元 (Factoryインデックス / Userファイル) で追跡する方式へ変更。
- **カーブ復元** — ステート/プリセットに CURVES が無い、または旧バージョンの場合は
  デフォルトへ戻す (前プリセットのカーブが残る事故を防止)。
- 存在しないファイルの読込、範囲外プリセットインデックス等のガードを追加。
- **キャプチャバッファのメモリ削減** — 30秒固定確保のため 192kHz で 46MB を
  常時確保していた。サンプル数上限 (1chあたり300万) を設けて**最大約24MB**に抑制。
  96kHz以下は従来通り30秒フル、192kHzで約15.6秒 (16小節/128BPM = 30秒相当なので
  実用上の不足はほぼ無い)。複数インスタンス立ち上げ時のメモリ消費が半減する。

### 追加: ファクトリープリセット +40 (計70種)

新カテゴリ **Scale Riser** (12種, スケールクオンタイズ活用) /
**Dubstep** (4) / **DnB** (3) / **Hardstyle** (2) / **Ambient** (4) を追加。
既存カテゴリにも EDM +4 / Trance +3 / Techno +3 / Cinematic +3 / Downer +2 を追加。

## v0.3.2 修正/追加

- **フィルターENVをフルレンジ化** — 従来±5octだったため20Hz起点でカーブ上端でも
  640Hz止まりだった。±10oct (20Hz..20kHz全域相当) に変更し、カーブ上端で
  ノブ位置に関わらず最大値へ到達する (ノブの帯表示も同スケール)。
- **ノブARC色のテーマ連動** — ノブは accentId を持ち、描画時に現在テーマから
  色を解決。テーマ変更が開いたままのウィンドウにも即反映される。
- **UI説明文を英語化** — 各タブのヒント/Limiter説明/バナー等。
- **ヘッダー再配置** — バージョン情報はCONFIGタブへ移動。ロゴ側に
  LIFT:AUTO/MANUALと全タブを左寄せし、右側に現在プリセット名と◀▶ボタン
  (Factory+Userを順送り) を配置。
- **ENVエディタ: Snap+補助線** — 各カーブ画面右上に CURVES / SNAP / 解像度コンボ
  (4/8/16/32/64分割)。SNAP ONでポイントX座標がグリッドへ吸着、補助線を表示。
- **カーブプリセット20種+Save/Load** — CURVESボタンから Linear/Exp/Log/S-Curve/
  Steps/Saw/Pulse/Zigzag等を即適用。"Save Current..."で名前を付けて保存し、
  User Curvesサブメニューから読込 (%APPDATA%/LIFT-X/Curves)。

## v0.3.1 修正

- ビルドエラー修正 (buildStateTreeのconst違反) / Font警告修正 / W3-W4競合解消
- **デフォルトを AUTO + LIFT 0% に変更** — 旧デフォルト(MANUAL+100%)では
  ENV評価位置が終端になり、StartKeyではなくEndKey側で発音されていた。
  ※ Pitch ENV自体はSt/EndKeyの範囲内で正確に動作 (カーブ下端=StartKey、
  上端=EndKey。COARSEノブは意図的なオフセットとして範囲外へ加算可能)
- **ENVをフルレンジ化** — 中央=ノブ現在値、上端=パラメーター最大値方向、
  下端=最小値方向 (クランプ付き)。ノブ0で下方向に描いても変化なし、
  ノブ0でも上端まで描けば最大値に到達する。ノブの帯表示もDSPと同一スケール。
- **ノブの変化幅表示を常時化** — 帯(白色・視認性向上)とライブドット(現在値)を
  常にノブへ表示し、再生中はリアルタイムに動く。

## v0.3 追加機能

- **LIFTノブの新仕様** — LIFT = 全マルチENVの評価位置(カーブのX座標)。
  - MANUAL: ノブ位置がそのままENV位置。68%で止めればその時点の音を維持し、
    ノブを動かさない限り変化しない。DAWオートメーション可能。
  - AUTO: Progress(0→1)がENV位置になり、ノブも連動して動く。
- **Bars拡張** — 1/32, 1/16, 1/8, 1/4, 1/2, 1, 2, 4, 8, 16小節。
- **録音長の厳密化** — キャプチャは「設定Bar分の本編 + FXテール1.5秒」で確定。
  鍵盤を押し続けてもBar数を超えて本編が録音されることはない。
- **MASTERエリア** — ノイズ列の下に OUT(最終音量) + CEILING(リミッター天井)。
- **マスターリミッター** — SPECTRA8のBrickLimiterを移植(瞬間アタック/レイテンシ0)。
  CEILING/RELEASE/ON-OFFをパラメーター化。
- **CONFIGタブ** — リミッター詳細設定 + カラーテーマ10種(Granular移植、
  グローバル設定に永続化。完全適用はウィンドウ開き直し)。
- **PRESETタブ** — NextGenKick2の3カラムブラウザを移植。カテゴリ(All/Factory/
  User/Favorites)、サブカテゴリ入力、プリセット名入力、★お気に入り、検索、
  右クリック削除に対応。
- **ファクトリープリセット30種埋め込み** — EDM/Trance/Bass/Techno/Cinematic
  各5種のライザー + Downer 5種。波形・ノイズ・フィルター・FX・カーブを網羅。

## v0.2.1 追加機能

- **LIFT MANUAL/AUTO** — ヘッダー(MAINタブ左)のトグル。押すたびに表示が
  「LIFT: MANUAL」⇔「LIFT: AUTO」に切り替わる。AUTO時はLIFTノブがProgressに
  連動して動的に動く (ノブは操作不可)。MANUAL時はDAWオートメーション可能。
- **ENV変化幅のノブ表示** — マルチENVが掛かるノブに、アーク色より濃い帯で
  変調範囲を表示し、白ドットで変調適用後の現在値を表示 (Granular ModMatrix方式)。
  対象: OSCのLEVEL/DETUNE/SPREAD、ノイズのLEVEL/PITCH/RES、FILTERのCUTOFF、
  FXのAMT/DRIVE/DEPTH/FB/SHIMMER/SHAPE (PitchENVは対応ノブなしのため対象外)。
- **ライザー波形表示 + WAV書き出し** — Progressバー下に録音波形を表示。
  ノートオンで録音開始(REC表示)、リリース+テール1.5秒で確定。ストリップを
  DAWへドラッグすると 32bit float WAV (セッションSR、最大30秒) としてドロップできる。
- **ブチ切れ/ノイズ対策** — 新規発音時のフィルター残留状態クリア、
  リトリガー時の2msデクリックランプ、アタック/リリース指数エンベロープ、
  ノートオン直後の平滑スナップ (古い値からのグライド防止)。

## スムージング一覧 (全ノブ精査済み)

| パラメーター | 平滑方式 |
|---|---|
| OSCピッチ (Key/COARSE/カーブ) | サンプル単位一次平滑 τ≒4ms (スケール量子化中は τ≒1.2ms) |
| OSC LEVEL / ノイズLEVEL | サンプル単位 τ≒4ms |
| WT POSITION | サンプル単位 τ≒4ms |
| DETUNE / SPREAD | ティックレート平滑 τ≒2.07ms (SR非依存) |
| LIFT (Manual/Auto両方) | ティックレート平滑 τ≒2.42ms (SR非依存) |
| FILTER CUTOFFカーブ | ティックレート平滑 τ≒1.45ms + ZDF/TPT (高速スイープ安定) |
| FILTER RES / ノイズRES | ティックレート平滑 τ≒2.07ms (SR非依存) |
| Sat AMT/DRIVE, Cho AMT/DEPTH, Rev SHIMMER, Duck AMT/SHAPE | サンプル単位 τ≒10ms (FxChain内) |
| Delay AMT/FB | サンプル単位 τ≒15ms (FX内部) |
| Delay TIME | サンプル単位 τ≒20ms (テープ式リピッチ) |
| Reverb Wet | FX内部平滑 (蓄積解放バースト防止) |
| Ducking ゲイン | 非対称平滑 (dip 2ms / 復帰 12ms) + PPQ位相同期 |
| MASTER | LinearSmoothedValue 20ms |
| アンプ | 指数A/R + リトリガーデクリック2ms |

(ATTACK/RELEASE/UNISON/WAVE種別/Key値は係数・離散値のため平滑対象外。
WAVE種別の切替は発音中に行うと波形が瞬時に変わります)

**v0.4以降、ティックレート系の平滑は実時間の時定数から係数を算出するため、
44.1kHz〜192kHzのどのサンプルレートでも同じ聴感になります。**
ノートオン時は上記すべてがターゲット値へ即スナップし、前ノートの残り値から
グライドすることはありません。

## コンセプト

MIDIノートオンをトリガーに、DAWのPPQ(トランスポート)へ同期して
指定Bar数 (1 / 2 / 4 / 8 / 16) で **Progress 0.0→1.0** を進め、
**31系統のマルチポイント・カーブ (マルチENV)** がピッチ・レベル・デチューン・
スプレッド・フィルター・FXパラメーターを一斉にスイープさせます。

## 構成 (v0.2 確定仕様)

| 項目 | 内容 |
|---|---|
| トリガー | ノート保持型。MIDIノートはトリガー専用 (音程には影響しない) |
| ピッチ | OSC毎に **StartKey/EndKey を絶対指定** (例 D3→G7)。ボタンを押してMIDI鍵盤で設定 (MIDIラーン)。Pitch ENVカーブ(下=Start/上=End)がスイープを描く |
| オシレーター | 3基。WAVEコンボ (Sine/Triangle/Square/Saw/FM/**Wavetable**) + POSITIONノブ + 波形表示。OSC毎に ON / **SOLO / MUTE** |
| カスタムWT | OSC毎に個別ロード。ブラウザ(サブフォルダ=カテゴリの2ペイン) + **RANDOM**。登録フォルダはグローバル設定へ永続化 (SPECTRA8方式) |
| ノイズ | White/Pink/Brown → 専用TPTバンドパス。SOLO/MUTE対応 |
| フィルター | ZDF/TPT SVF ×4 (LP/HP/BP/Notch)。**ソース別ルーティング**: 各フィルターにOSC1-3/ノイズを個別に通す/バイパスする点灯式ボタン (内部は4×4=16基のSVF) |
| FX | 5スロット (適用順序を選択)。Saturation / Chorus / Delay / Reverb / Ducking。Duckingレートは 1Bar〜1/64 (付点・三連対応, PPQ同期)。**エフェクト毎にソース別ルーティング** (OSC1-3/Noiseを個別にバイパス) |
| マルチENV | 31カーブ: OSC1-3 (PITCH/LEVEL/DETUNE/SPREAD) + ノイズ (PITCH/LEVEL/RES) + FILTER1-4 + FX (Sat:AMT,DRIVE / Cho:AMT,DEPTH / Dly:AMT,FB,TIME / Rev:AMT,SHIMMER / Duck:AMT,RATE,SHAPE) |
| ENVの効き方 | **バイポーラ加算式**: カーブ中央=ノブ現在値、上下でパラメーターレンジ半分を±加算 (クランプ付き)。OSCピッチのみ Start→End のユニポーラ補間。Delay TIME/Duck RATEは±2オクターブの拍長変調 (下げるほど加速) |
| 単位表示 | 簡素化 (%, Hz/k, ms/s, st, ct, dB, oct, ノート名)。内部解像度はフル |
| GUI | タブ方式: MAIN / **OSC ENV** (ソース×ターゲットの入れ子タブ) / FILTER / FX (FX毎ENV入れ子タブ)。ダークテーマ×パステル |
| SR対応 | 44.1〜192kHz (全バッファをprepareToPlayでSR依存確保) |

### カーブエディタ操作

- ポイントをドラッグ: 移動 (端点はX固定) / 空白ダブルクリック: 追加 (最大32)
- ポイントをダブルクリック / Alt+クリック: 削除
- セグメント中央の◆をドラッグ: テンション調整、ダブルクリックでリセット
- ノブは右クリックで数値直接入力

## リアルタイム安全設計

- `processBlock` 内のメモリアロケーション/ロック一切なし (prepareToPlayで事前確保)
- カーブのGUI→オーディオ通信はatomicインデックスのロックフリーSPSC (リング8面×31)
- DAWフェイルセーフ: SR/ブロックサイズ不一致検知で即ゼロクリア+リセット (Ableton Live対策)
- カーブはAPVTS外で管理 → ホストオートメーション巻き戻りが構造的に発生しない
- MIDIラーンはaudio→GUIをatomicカウンタで通知し、パラメーター変更はGUIスレッドから実行
- GUI: `juce::VBlankAttachment` によるプレイヘッド同期アニメーション、paint内のPath再確保なし

### 計画書からの変更点

- `juce_audio_processors_headless` はJUCE 8に存在しないため、標準モジュール構成 +
  ソースレベルのDSP/GUI完全分離で同目的を達成
- SIMDはSoAレイアウト+自動ベクトル化のスカラー設計 (SPECTRA8の知見に準拠)
- v0.2でFreezeを削除、Duckingを追加 (5スロット順序選択式)

## ビルド

```
cmake -B build -DJUCE_PATH=C:/JUCE
cmake --build build --config Release
```

(Visual Studio 2022 の「フォルダーを開く」+ CMakeSettings でも可)

## ファイル構成

```
Source/
  PluginProcessor.h/.cpp   プロセッサー (APVTS/フェイルセーフ/DAW同期/Learn/グローバル設定)
  PluginEditor.h/.cpp      タブ式エディタ (MAIN/OSC ENV/FILTER/FX)
  DSP/                     (GUI非依存)
    Wavetable.h            MorphWavetable (SPECTRA8移植, ビルトイン/カスタム選択API)
    CurveData.h            31系統マルチENV + ロックフリーCurveStore
    ZdfFilter.h            TPT/ZDF SVF (Simper)
    ScaleQuantizer.h       スケール量子化 (70種の音階テーブル)
    RiserEngine.h          シンセコア (Key指定ピッチ/Solo/Mute/ソース別ルーティング)
    FxChain.h              5スロットFX (Sat/Cho/Dly/Rev/Duck, PPQ同期Ducking)
  GUI/
    ColorPalette.h         ダークテーマ (Granular Midnight系)
    ArcDial / ValueKnob / GlowToggle
    CurveEditor.h/.cpp     マルチポイントカーブエディタ (VBlank同期)
    WaveDisplay.h          OSC毎の波形表示
    PitchRail.h            Pitch ENV ライブ表示 (音名+スケール目盛り, VBlank同期)
    WavetableBrowser.h/.cpp カテゴリ式WTブラウザ (RANDOM/FACTORY/ADD DIR)
    MainPanel / OscEnvPanel / FilterPanel / FxPanel
```
