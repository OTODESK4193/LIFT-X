# LIFT-X — Riser MIDI Synthesizer (OTODESK)

ライザー専用MIDIシンセサイザー。JUCE 8.0系 / VST3 + Standalone。
開発計画書「プロジェクト LIFT-X」に基づく実装。

## コンセプト

MIDIノートオンをトリガーに、DAWのPPQ(トランスポート)へ同期して
指定Bar数 (1 / 2 / 4 / 8 / 16) で **Progress 0.0→1.0** を進め、
9系統の**マルチポイント・カーブ (マルチENV)** がピッチ・フィルター・FXを
一斉にスイープさせます。

## 構成 (確定仕様)

| 項目 | 内容 |
|---|---|
| トリガー | ノート保持型 (完走後ホールド / ノートオフでリリース+リセット) |
| マルチENV | 固定9系統: Pitch×4 (Osc1/2/3/Noise) + Filter×4 + FX×1 |
| オシレーター | 3基 MorphWavetable (Sine→Tri→Square→Saw→FM) + カスタムWT (SPECTRA8方式, 2048smp/frame) |
| ノイズ | White/Pink/Brown → 専用TPTバンドパスでピッチスイープ |
| フィルター | ZDF/TPT SVF ×4直列 (LP/HP/BP/Notch, カーブで±5oct変調) |
| FX | 6スロット直列: Saturation(ADAA) / Chorus / Tape Delay / Freeze / Shimmer Reverb / Beat Ducking(PPQ同期) |
| GUI | タブ方式 (MAIN / PITCH / FILTER / FX)、ダークテーマ×パステル |
| SR対応 | 44.1〜192kHz (全バッファをprepareToPlayでSR依存確保) |

## タブ構成

- **MAIN** — LIFTノブ(全カーブ強度)、BARS、ATTACK/RELEASE/MASTER、
  OSC1-3 (WAVE/LEVEL/COARSE/UNISON/DETUNE/SPREAD)、NOISE、カスタムWTロード、Progress表示
- **PITCH** — サブタブ OSC1/OSC2/OSC3/NOISE のカーブ + RANGEノブ
- **FILTER** — サブタブ FLT1-4 のカーブ + ENABLE/TYPE/CUTOFF/RES/ENV AMT
- **FX** — 6スロット (TYPE/AMT/ENV) + 各FX詳細 + FXカーブ

### カーブエディタ操作

- ポイントをドラッグ: 移動 (端点はX固定)
- 空白をダブルクリック: ポイント追加 (最大32)
- ポイントをダブルクリック / Alt+クリック: 削除
- セグメント中央の◆をドラッグ: テンション(カーブ形状)調整、ダブルクリックでリセット
- ノブは右クリックで数値直接入力

## リアルタイム安全設計 (計画書準拠)

- `processBlock` 内のメモリアロケーション/ロックは一切なし
  (全バッファは `prepareToPlay` で事前確保)
- カーブのGUI→オーディオ通信はatomicインデックスによるロックフリーSPSC
  (`CurveStore`: リング8面、旧スナップショットは上書きまで保持)
- DAWフェイルセーフ: SR/ブロックサイズ不一致検知で即ゼロクリア+リセット
  (Ableton LiveのSR変更時の先行processBlock対策)
- カーブはAPVTS外で管理 → ホストオートメーションから完全隔離
  (計画書の `withAutomatable(false)` 方針の強化版。LIFTノブ操作による
  オートメーション巻き戻りが構造的に発生しない)
- GUI: `juce::VBlankAttachment` によるリフレッシュ同期アニメーション
  (デストラクタで自動解除)、paint内のPath再確保なし (Golden Rule)

### 計画書からの変更点

- `juce_audio_processors_headless` はJUCE 8に存在しないモジュール名のため、
  標準モジュール構成 + ソースレベルのDSP/GUI完全分離
  (DSPフォルダはGUIヘッダを一切includeしない) で同目的を達成
- SIMDはSPECTRA8の知見に合わせ「SoAレイアウト+タイトループの自動ベクトル化」
  によるスカラー設計 (`juce::dsp::SIMDRegister`不使用、`/fp:fast /O2`)
- FXは選択に基づきGranularの5種を移植し、独立Beat Duckingスロットを追加

## ビルド

```
cmake -B build -DJUCE_PATH=C:/JUCE
cmake --build build --config Release
```

(SPECTRA8と同様、Visual Studio 2022の「フォルダーを開く」+ CMakeSettings でも可)

## ファイル構成

```
Source/
  PluginProcessor.h/.cpp   プロセッサー (APVTS / フェイルセーフ / DAW同期)
  PluginEditor.h/.cpp      タブ式エディタ
  DSP/                     (GUI非依存)
    Wavetable.h            MorphWavetable (SPECTRA8移植, カスタムWT対応)
    CurveData.h            マルチENVデータモデル + ロックフリーCurveStore
    ZdfFilter.h            TPT/ZDF SVF (Simper)
    RiserEngine.h          シンセコア + PPQ同期Progressエンジン
    FxChain.h              6スロットFX (Granular移植 + Beat Ducking)
  GUI/
    ColorPalette.h         ダークテーマ (Granular Midnight系)
    ArcDial.h/.cpp         アークダイアルLnF
    ValueKnob.h            右クリック数値入力ノブ
    GlowToggle.h           LEDトグル
    CurveEditor.h/.cpp     マルチポイントカーブエディタ (VBlank同期)
    MainPanel / PitchPanel / FilterPanel / FxPanel
```
