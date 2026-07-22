# LIFT-X — Riser MIDI Synthesizer (OTODESK)

ライザー専用MIDIシンセサイザー。JUCE 8.0系 / VST3 + Standalone。v0.2。

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
| FX | 5スロット (適用順序を選択)。Saturation / Chorus / Delay / Reverb / Ducking。Duckingレートは 1Bar〜1/64 (付点・三連対応, PPQ同期) |
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
    RiserEngine.h          シンセコア (Key指定ピッチ/Solo/Mute/ソース別ルーティング)
    FxChain.h              5スロットFX (Sat/Cho/Dly/Rev/Duck, PPQ同期Ducking)
  GUI/
    ColorPalette.h         ダークテーマ (Granular Midnight系)
    ArcDial / ValueKnob / GlowToggle
    CurveEditor.h/.cpp     マルチポイントカーブエディタ (VBlank同期)
    WaveDisplay.h          OSC毎の波形表示
    WavetableBrowser.h/.cpp カテゴリ式WTブラウザ (RANDOM/FACTORY/ADD DIR)
    MainPanel / OscEnvPanel / FilterPanel / FxPanel
```
