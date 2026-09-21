// ==========================================
// File: FactoryPresets.cpp
// 即戦力の Riser / Downer プリセット 70種 (コード埋め込み)
//
//  記法:
//   params: "id=value;..." (Choice/Boolはインデックス/0-1)
//   curves: "idx:x,y,c;x,y,c|idx:..."
//  主要インデックス:
//   Wave: 0=Sine 1=Tri 2=Sqr 3=Saw 4=FM 5=Wavetable
//   noiseType: 0=White 1=Pink 2=Brown
//   fltType: 0=LP 1=HP 2=BP 3=Notch 4=Vowel 5=Comb
//   fxNType: 0=None 1=Sat 2=Cho 3=Dly 4=Rev 5=Duck 6=Stutter
//   satAlgo: 0=SoftTanh 1=HardClip 2=Triode 3=Tape 4=Transformer
//            5=JFET 6=BJT 7=Wavefold 8=Exciter 9=Cubic
//   bars: 0=1/32 1=1/16 2=1/8 3=1/4 4=1/2 5=1 6=2 7=4 8=8 9=16
//   dlyTime: 0=1/2 2=1/4 5=1/8 8=1/16 / duckRate: 5=1/4 8=1/8 11=1/16 14=1/32
//   カーブidx (CurveStore::Index と 1:1 対応。必ず enum 側を正とすること):
//              0=O1Pitch  1=O1Level  2=O1Det   3=O1Spread
//              4=O2Pitch  5=O2Level  6=O2Det   7=O2Spread
//              8=O3Pitch  9=O3Level 10=O3Det  11=O3Spread
//             12=NzPitch 13=NzLevel 14=NzRes
//             15-18=Flt1-4 Cutoff
//             19-22=Flt1-4 Res          ← v0.4 で追加 (ここが挿入されたため以降が+4)
//             23=SatAmt  24=SatDrive
//             25=ChoAmt  26=ChoDepth
//             27=DlyAmt  28=DlyFb    29=DlyTime
//             30=RevAmt  31=RevShimmer
//             32=DuckAmt 33=DuckRate 34=DuckShape
//             35-38=Pan (OSC1,OSC2,OSC3,Noise)   ← v0.6 追加
//             39=StutAmt 40=StutRate             ← v0.6 追加
//             41-43=Pos (OSC1,OSC2,OSC3)         ← v1.0.1 追加
//             (kNumCurves = 44)
//
//   カーブの REPEAT (LFO化) は先頭に "R<n>;" を付ける。
//    例) "1:R16;0,0.9,0;0.45,0.9,0;0.5,0.1,0;1,0.1,0"  = 16分のゲート
//    ※ 区切りが ';' なのは、カーブ同士の区切り '|' と衝突させないため。
//
//  ※ 履歴: v0.4 で Filter1Res..Filter4Res が 19-22 へ挿入された際、
//     本ファイルのプリセットデータが旧レイアウト (19=SatAmt...) のまま残り、
//     70件中49件が誤ったカーブへ書き込まれていた (例: 26 は RevAmt のつもりが
//     ChoDepth、19 は SatAmt のつもりが Filter1Res へ)。
//     v0.4.3 で全データを +4 補正済み。今後 CurveStore::Index に追加する場合は
//     必ず末尾へ追加し、途中挿入はしないこと。
//
//  ---- Scaleクオンタイズ (v0.4) ----
//   scaleOn=1 でPitch ENVがKey+Scaleの構成音へスナップする (階段状ライザー)。
//   scaleKey: 0=C 1=C# 2=D 3=D# 4=E 5=F 6=F# 7=G 8=G# 9=A 10=A# 11=B
//   scaleType (ScaleQuantizer::getScales() と同順):
//     0=Chromatic 1=Major 2=NaturalMinor 3=MajorPent 4=MinorPent 5=Dorian
//     6=Lydian 7=Mixolydian 8=Phrygian 9=HarmonicMinor 10=MelodicMinor
//     11=WholeTone 12=Oct&5th 13=Quartal 14=Sus2/4 15=Dim7 16=Locrian
//     17=BluesMinor 18=BluesMajor 19=HarmonicMajor 20=DoubleHarmonic
//     21=PhrygianDom 22=LydianDom 23=LydianAug 24=Mixob6 25=HalfDim
//     26=Altered 27=Dorianb2 28=Dorian#4 29=Lydian#2 30=UltraLocrian
//     31=HungarianMinor 32=HungarianMajor 33=NeapolitanMinor 34=NeapolitanMajor
//     35=Enigmatic 36=Persian 37=Oriental 38=NineTone 39=Spanish8
//     40=Todi 41=Marva 42=Purvi 43=AhirBhairav
//     44=Hirajoshi 45=InSen 46=Iwato 47=Kumoi 48=Yo 49=MiyakoBushi
//     50=Ryukyu 51=ChineseJiao 52=EgyptianPent 53=BalinesePelog
//     54=Prometheus 55=Tritone 56=Augmented 57=HalfWholeDim 58=WholeHalfDim
//     59=BebopDom 60=BebopMajor 61=MajTriad 62=MinTriad 63=Sus4Triad
//     64=Maj7 65=Min7 66=Dom7 67=Min9 68=Maj9 69=OctavesOnly
//   osc{N}Scale=0 でそのOSCだけ量子化を外せる (ノンクオンタイズ層を重ねる用途)。
// ==========================================
#include "FactoryPresets.h"
#include "PluginProcessor.h"

namespace FactoryPresets
{

const std::vector<Item>& items()
{
    static const std::vector<Item> list = {

    // ================= EDM =================
    { "EDM", "Classic Saw Riser",
      "osc1Wave=3;osc1Uni=7;osc1Det=30;osc1Spread=1;osc1KeyStart=36;osc1KeyEnd=96;"
      "flt1Cutoff=600;flt1Res=1.5;flt1Env=0.8;bars=7;release=350;"
      "fx1Type=4;revAmt=0.35;revDecay=0.75;revShimmer=0.3;fx2Type=5;duckAmt=0.4;duckRate=5",
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|30:0,0.4,0.3;1,0.9,0" },

    { "EDM", "White Sweep Up",
      "osc1On=0;noiseType=0;noiseLevel=0.95;noisePitch=250;noiseRes=3.5;noiseRange=6;"
      "flt1On=0;bars=7;release=500;fx1Type=4;revAmt=0.45;revDecay=0.8",
      "12:0,0.35,0.4;1,1,0|13:0,0.25,0.4;1,0.95,0|30:0,0.4,0;1,0.9,0" },

    { "EDM", "Pluck Echo Build",
      "osc1Wave=2;osc1Uni=3;osc1Det=15;osc1KeyStart=60;osc1KeyEnd=84;attack=0.5;release=400;"
      "bars=8;fx1Type=3;dlyAmt=0.5;dlyTime=5;dlyFb=0.65;fx2Type=5;duckAmt=0.5;duckRate=8",
      "0:0,0,0.3;1,1,0|15:0,0.5,0.3;1,1,0|27:0,0.35,0.4;1,0.95,0" },

    { "EDM", "Big Room Lift",
      "osc1Wave=3;osc1Uni=7;osc1Det=45;osc1Spread=1;osc1KeyStart=33;osc1KeyEnd=93;"
      "osc2On=1;osc2Wave=3;osc2Coarse=12;osc2Uni=5;osc2Det=25;osc2Level=0.5;"
      "osc2KeyStart=33;osc2KeyEnd=93;flt1Cutoff=400;flt1Env=0.9;flt1Res=2.5;bars=8;"
      "fx1Type=1;satAmt=0.4;satDrive=4;fx2Type=4;revAmt=0.4",
      "0:0,0,0.55;1,1,0|4:0,0,0.55;1,1,0|15:0,0.4,0.5;1,1,0|30:0,0.42,0.2;1,0.85,0" },

    { "EDM", "Noise Roll Accelerator",
      "osc1On=0;noiseType=0;noiseLevel=0.9;noisePitch=1500;noiseRes=1.2;noiseRange=3;flt1On=0;"
      "bars=6;fx1Type=5;duckAmt=0.85;duckRate=8;duckShape=3;fx2Type=4;revAmt=0.3",
      "13:0,0.3,0.4;1,0.95,0|33:0,0.5,0;1,0.1,0|12:0,0.4,0.3;1,0.9,0" },

    // ================= Trance =================
    { "Trance", "Supersaw Heaven",
      "osc1Wave=3;osc1Uni=7;osc1Det=55;osc1Spread=1;osc1Level=0.75;osc1KeyStart=45;osc1KeyEnd=81;"
      "osc2On=1;osc2Wave=3;osc2Uni=7;osc2Det=35;osc2Coarse=12;osc2Level=0.45;"
      "osc2KeyStart=45;osc2KeyEnd=81;flt1Cutoff=700;flt1Env=0.85;flt1Res=1.2;bars=8;release=600;"
      "fx1Type=2;choAmt=0.5;choDepth=0.6;fx2Type=4;revAmt=0.5;revShimmer=0.55",
      "0:0,0,0.6;1,1,0|4:0,0,0.6;1,1,0|15:0,0.45,0.5;1,1,0|30:0,0.45,0.3;1,0.9,0" },

    { "Trance", "Acid Climb",
      "osc1Wave=3;osc1KeyStart=36;osc1KeyEnd=72;flt1Cutoff=300;flt1Res=8;flt1Env=0.95;bars=7;"
      "fx1Type=1;satAmt=0.55;satDrive=5;satAlgo=2;fx2Type=3;dlyAmt=0.35;dlyTime=8;dlyFb=0.5",
      "0:0,0,0.4;1,1,0|15:0,0.4,0;0.25,0.7,-0.5;0.5,0.5,0;0.75,0.85,-0.5;1,1,0" },

    { "Trance", "Gated Riser",
      "osc1Wave=3;osc1Uni=5;osc1Det=30;osc1KeyStart=45;osc1KeyEnd=93;bars=8;"
      "fx1Type=5;duckAmt=0.7;duckRate=11;duckShape=4;fx2Type=4;revAmt=0.4",
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|32:0,0.6,0;1,0.35,0" },

    { "Trance", "Uplifter 16Bars",
      "osc1Wave=3;osc1Uni=7;osc1Det=25;osc1KeyStart=45;osc1KeyEnd=93;"
      "osc3On=1;osc3Wave=0;osc3Coarse=-12;osc3Level=0.5;osc3KeyStart=45;osc3KeyEnd=93;"
      "noiseType=1;noiseLevel=0.35;bars=9;release=800;"
      "fx1Type=4;revAmt=0.5;revShimmer=0.7;revDecay=0.85",
      "0:0,0,0.7;1,1,0|8:0,0,0.7;1,1,0|13:0,0.3,0.6;1,0.9,0|15:0,0.42,0.6;1,1,0|30:0,0.42,0.4;1,0.95,0" },

    { "Trance", "Dream Shimmer",
      "osc1Wave=1;osc1Uni=3;osc1Det=18;osc1KeyStart=48;osc1KeyEnd=84;flt1Cutoff=1200;flt1Env=0.6;"
      "bars=8;release=900;fx1Type=4;revAmt=0.6;revShimmer=0.8;revDecay=0.85;fx2Type=2;choAmt=0.4",
      "0:0,0,0.45;1,1,0|15:0,0.5,0.3;1,1,0|31:0,0.4,0.3;1,0.95,0" },

    // ================= Bass =================
    { "Bass", "Growl Ramp",
      "osc1Wave=4;osc1KeyStart=24;osc1KeyEnd=60;osc2On=1;osc2Wave=2;osc2Level=0.5;"
      "osc2KeyStart=24;osc2KeyEnd=60;flt1Cutoff=250;flt1Res=5;flt1Env=0.9;bars=6;"
      "fx1Type=1;satAmt=0.6;satDrive=6;satAlgo=7",
      "0:0,0,0;0.5,0.4,0;0.75,0.7,0;1,1,0|4:0,0,0;0.5,0.4,0;0.75,0.7,0;1,1,0|15:0,0.4,0.3;1,1,0|24:0,0.4,0;1,0.85,0" },

    { "Bass", "Sub Rise",
      "osc1Wave=0;osc1Level=0.95;osc1KeyStart=24;osc1KeyEnd=48;noiseType=2;noiseLevel=0.5;"
      "noisePitch=150;noiseRange=2.5;flt1On=0;bars=5;fx1Type=5;duckAmt=0.5;duckRate=5",
      "0:0,0,0.4;1,1,0|13:0,0.35,0.3;1,0.8,0" },

    { "Bass", "Talky Sweep",
      "osc1Wave=2;osc1Uni=3;osc1Det=20;osc1KeyStart=36;osc1KeyEnd=72;"
      "flt1Type=2;flt1Cutoff=400;flt1Res=9;flt1Env=0.95;"
      "flt2On=1;flt2Type=2;flt2Cutoff=1200;flt2Res=6;flt2Env=-0.6;bars=6;"
      "fx1Type=1;satAmt=0.35;satDrive=3.5",
      "0:0,0,0.4;1,1,0|15:0,0.4,0.4;1,1,0|16:0,0.5,-0.4;1,0.95,0" },

    { "Bass", "Machine Gun Duck",
      "osc1On=0;noiseType=0;noiseLevel=0.8;noisePitch=800;noiseRange=4;flt1On=0;bars=7;"
      "fx1Type=5;duckAmt=0.9;duckRate=11;duckShape=5;fx2Type=1;satAmt=0.3;satDrive=3",
      "12:0,0.4,0.4;1,0.95,0|13:0,0.3,0.4;1,0.95,0|33:0,0.5,0;0.7,0.3,0;1,0.05,0" },

    { "Bass", "Metal FM Screech",
      "osc1Wave=4;osc1Uni=5;osc1Det=40;osc1KeyStart=48;osc1KeyEnd=96;"
      "flt1Type=1;flt1Cutoff=200;flt1Env=0.5;bars=7;"
      "fx1Type=1;satAmt=0.5;satDrive=7;satAlgo=8;fx2Type=4;revAmt=0.35",
      "0:0,0,0.55;1,1,0|15:0,0.5,0.3;1,1,0|23:0,0.4,0.3;1,0.85,0" },

    // ================= Techno =================
    { "Techno", "Warehouse Lift",
      "osc1Wave=3;osc1Uni=5;osc1Det=35;osc1KeyStart=33;osc1KeyEnd=69;"
      "noiseType=1;noiseLevel=0.4;flt1Cutoff=500;flt1Env=0.85;flt1Res=3;bars=8;"
      "fx1Type=3;dlyAmt=0.4;dlyTime=5;dlyFb=0.55;dlyDamp=0.5;fx2Type=4;revAmt=0.35",
      "0:0,0,0.5;1,1,0|13:0,0.3,0.4;1,0.85,0|15:0,0.42,0.5;1,1,0|27:0,0.4,0.3;1,0.9,0" },

    { "Techno", "Rumble Riser",
      "osc1Wave=0;osc1Level=0.9;osc1KeyStart=24;osc1KeyEnd=36;noiseType=2;noiseLevel=0.7;"
      "noisePitch=120;noiseRes=1.5;noiseRange=2.5;flt1Cutoff=300;flt1Env=0.6;bars=7;"
      "fx1Type=1;satAmt=0.5;satDrive=5;satAlgo=3",
      "0:0,0,0.4;1,1,0|12:0,0.4,0.3;1,0.9,0|13:0,0.4,0.3;1,0.9,0|15:0,0.45,0.3;1,1,0" },

    { "Techno", "Percussive Climb",
      "osc1Wave=2;osc1KeyStart=60;osc1KeyEnd=84;attack=0.3;release=120;bars=6;"
      "fx1Type=5;duckAmt=0.8;duckRate=11;duckShape=6;fx2Type=3;dlyAmt=0.45;dlyTime=8;dlyFb=0.6",
      "0:0,0,0.3;1,1,0|15:0,0.5,0.3;1,1,0|32:0,0.55,0;1,0.3,0" },

    { "Techno", "Hypnotic Notch",
      "osc1Wave=1;osc1Uni=3;osc1Det=22;osc1KeyStart=45;osc1KeyEnd=69;"
      "flt1Type=3;flt1Cutoff=800;flt1Res=4;flt1Env=0.9;"
      "flt2On=1;flt2Cutoff=2000;flt2Env=0.5;bars=8;fx1Type=2;choAmt=0.45;choDepth=0.5",
      "0:0,0,0.4;1,1,0|15:0,0.3,0;0.5,0.9,-0.4;1,0.4,0|16:0,0.45,0.4;1,1,0" },

    { "Techno", "Industrial Noise Wall",
      "osc1On=0;noiseType=0;noiseLevel=1;noisePitch=600;noiseRes=0.8;noiseRange=7;"
      "flt1Type=1;flt1Cutoff=100;flt1Env=0.8;bars=8;"
      "fx1Type=1;satAmt=0.55;satDrive=6;fx2Type=4;revAmt=0.45;revDamp=0.6",
      "12:0,0.3,0.5;1,1,0|13:0,0.35,0.5;1,1,0|15:0,0.4,0.5;1,1,0|23:0,0.4,0.2;1,0.8,0" },

    // ================= Cinematic =================
    { "Cinematic", "Trailer Swell",
      "osc1Wave=3;osc1Uni=7;osc1Det=20;osc1KeyStart=29;osc1KeyEnd=65;"
      "osc3On=1;osc3Wave=0;osc3Coarse=-12;osc3Level=0.6;osc3KeyStart=29;osc3KeyEnd=65;"
      "noiseType=1;noiseLevel=0.45;attack=200;release=1200;bars=8;"
      "flt1Cutoff=350;flt1Env=0.9;fx1Type=4;revAmt=0.65;revDecay=0.9;revShimmer=0.5",
      "0:0,0,0.6;1,1,0|8:0,0,0.6;1,1,0|13:0,0.3,0.5;1,0.95,0|15:0,0.4,0.6;1,1,0|30:0,0.45,0.4;1,0.95,0" },

    { "Cinematic", "Braam Riser",
      "osc1Wave=3;osc1Uni=7;osc1Det=60;osc1KeyStart=24;osc1KeyEnd=48;bars=7;release=1000;"
      "flt1Cutoff=450;flt1Env=0.7;fx1Type=1;satAmt=0.6;satDrive=8;satAlgo=1;fx2Type=4;revAmt=0.5;revDecay=0.8",
      "0:0,0,0.35;1,1,0|15:0,0.42,0.4;1,1,0|24:0,0.45,0.2;1,0.8,0" },

    { "Cinematic", "Ethereal Rise",
      "osc1Wave=0;osc1Uni=5;osc1Det=15;osc1KeyStart=60;osc1KeyEnd=96;"
      "noiseType=1;noiseLevel=0.2;noiseRange=6;bars=9;release=1500;flt1On=0;"
      "fx1Type=4;revAmt=0.7;revShimmer=0.85;revDecay=0.9;fx2Type=2;choAmt=0.5",
      "0:0,0,0.7;1,1,0|12:0,0.4,0.5;1,1,0|13:0,0.3,0.5;1,0.8,0|30:0,0.5,0.3;1,0.95,0" },

    { "Cinematic", "Tension Strings",
      "osc1Wave=3;osc1Uni=7;osc1Det=18;osc1Spread=0.9;osc1KeyStart=52;osc1KeyEnd=76;"
      "flt1Type=2;flt1Cutoff=900;flt1Res=5;flt1Env=0.6;bars=8;"
      "fx1Type=5;duckAmt=0.6;duckRate=8;duckShape=2;fx2Type=4;revAmt=0.5",
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|32:0,0.5,0;1,0.75,0" },

    { "Cinematic", "Sub Boom Rise",
      "osc1Wave=0;osc1KeyStart=24;osc1KeyEnd=31;noiseType=2;noiseLevel=0.35;noiseRange=2;"
      "flt1On=0;bars=6;release=1500;fx1Type=1;satAmt=0.4;satDrive=4",
      "0:0,0,0.3;1,1,0|13:0,0.4,0.4;1,0.85,0" },

    // ================= Downer =================
    { "Downer", "Classic Downer",
      "osc1Wave=3;osc1Uni=7;osc1Det=30;osc1KeyStart=96;osc1KeyEnd=36;bars=6;release=600;"
      "flt1Cutoff=4000;flt1Env=-0.7;fx1Type=4;revAmt=0.4",
      "0:0,0,0.4;1,1,0|15:0,0.5,0.4;1,1,0" },

    { "Downer", "Noise Fall",
      "osc1On=0;noiseType=0;noiseLevel=0.9;noisePitch=6000;noiseRes=2.5;noiseRange=6;"
      "flt1On=0;bars=5;release=700;fx1Type=4;revAmt=0.5;revDecay=0.8",
      "12:0,0.9,-0.4;1,0.1,0|13:0,0.9,0;1,0.3,0" },

    { "Downer", "Laser Drop",
      "osc1Wave=0;osc1KeyStart=108;osc1KeyEnd=36;bars=4;release=400;flt1On=0;"
      "fx1Type=3;dlyAmt=0.4;dlyTime=8;dlyFb=0.6",
      "0:0,0,-0.6;1,1,0|27:0,0.5,0;1,0.8,0" },

    { "Downer", "Tape Stop",
      "osc1Wave=3;osc1Uni=5;osc1Det=25;osc1KeyStart=60;osc1KeyEnd=24;bars=4;release=300;"
      "flt1Cutoff=3000;flt1Env=-0.8;fx1Type=1;satAmt=0.5;satDrive=5;satAlgo=3",
      "0:0,0,0.7;1,1,0|15:0,0.5,0.5;1,1,0" },

    { "Downer", "Reverse Shimmer Down",
      "osc1Wave=1;osc1Uni=3;osc1Det=20;osc1KeyStart=84;osc1KeyEnd=48;"
      "noiseType=1;noiseLevel=0.3;bars=7;release=1200;flt1Cutoff=2500;flt1Env=-0.5;"
      "fx1Type=4;revAmt=0.65;revShimmer=0.8;revDecay=0.88;fx2Type=2;choAmt=0.4",
      "0:0,0,0.45;1,1,0|13:0,0.7,0;1,0.25,0|15:0,0.5,0.3;1,1,0|30:0,0.45,0.3;1,0.9,0" },

    // ============================================================
    //  v0.4 追加分 (40種)
    // ============================================================

    // ================= Scale Riser (スケールクオンタイズ活用) =================
    { "Scale Riser", "Minor Staircase",
      "scaleOn=1;scaleKey=0;scaleType=2;"
      "osc1Wave=3;osc1Uni=5;osc1Det=22;osc1Spread=0.9;osc1KeyStart=36;osc1KeyEnd=84;"
      "flt1Cutoff=700;flt1Res=1.8;flt1Env=0.8;bars=8;release=400;"
      "fx1Type=4;revAmt=0.35;revDecay=0.75;fx2Type=5;duckAmt=0.4;duckRate=5",
      "0:0,0,0.35;1,1,0|15:0,0.45,0.4;1,1,0|30:0,0.4,0.2;1,0.85,0" },

    { "Scale Riser", "Pentatonic Ladder",
      "scaleOn=1;scaleKey=9;scaleType=4;"
      "osc1Wave=2;osc1Uni=3;osc1Det=14;osc1KeyStart=45;osc1KeyEnd=93;attack=1;release=300;"
      "bars=8;flt1Cutoff=900;flt1Res=2.5;flt1Env=0.7;"
      "fx1Type=3;dlyAmt=0.45;dlyTime=8;dlyFb=0.6;dlyDuck=0.6;fx2Type=4;revAmt=0.3",
      "0:0,0,0.2;1,1,0|15:0,0.5,0.3;1,1,0|27:0,0.35,0.4;1,0.9,0" },

    { "Scale Riser", "Harmonic Minor Climb",
      "scaleOn=1;scaleKey=2;scaleType=9;"
      "osc1Wave=3;osc1Uni=7;osc1Det=32;osc1Spread=1;osc1KeyStart=38;osc1KeyEnd=86;"
      "osc2On=1;osc2Wave=0;osc2Coarse=12;osc2Level=0.45;osc2KeyStart=38;osc2KeyEnd=86;"
      "flt1Cutoff=550;flt1Res=3;flt1Env=0.9;bars=8;release=600;"
      "fx1Type=1;satAmt=0.35;satDrive=3.5;fx2Type=4;revAmt=0.45;revShimmer=0.5",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|15:0,0.4,0.5;1,1,0|30:0,0.42,0.3;1,0.9,0" },

    { "Scale Riser", "Hirajoshi Ascent",
      "scaleOn=1;scaleKey=4;scaleType=44;"
      "osc1Wave=1;osc1Uni=3;osc1Det=16;osc1KeyStart=52;osc1KeyEnd=88;"
      "noiseType=1;noiseLevel=0.22;noisePitch=2500;noiseRange=4;"
      "bars=8;release=900;flt1Cutoff=1400;flt1Env=0.55;"
      "fx1Type=4;revAmt=0.6;revShimmer=0.75;revDecay=0.88;fx2Type=2;choAmt=0.4",
      "0:0,0,0.55;1,1,0|13:0,0.25,0.5;1,0.85,0|15:0,0.5,0.3;1,1,0|30:0,0.45,0.3;1,0.95,0" },

    { "Scale Riser", "Iwato Tension",
      "scaleOn=1;scaleKey=7;scaleType=46;"
      "osc1Wave=4;osc1Uni=3;osc1Det=24;osc1KeyStart=43;osc1KeyEnd=79;"
      "flt1Type=2;flt1Cutoff=800;flt1Res=6;flt1Env=0.8;bars=7;"
      "fx1Type=1;satAmt=0.45;satDrive=5;satAlgo=5;fx2Type=3;dlyAmt=0.35;dlyTime=5;dlyFb=0.5",
      "0:0,0,0.3;1,1,0|15:0,0.4,0.4;1,1,0|23:0,0.4,0.2;1,0.85,0" },

    { "Scale Riser", "Phrygian Dominant Rise",
      "scaleOn=1;scaleKey=4;scaleType=21;"
      "osc1Wave=3;osc1Uni=7;osc1Det=40;osc1Spread=1;osc1KeyStart=40;osc1KeyEnd=88;"
      "flt1Cutoff=450;flt1Res=4;flt1Env=0.92;bars=8;release=500;"
      "fx1Type=1;satAmt=0.5;satDrive=6;satAlgo=2;fx2Type=4;revAmt=0.4;fx3Type=5;duckAmt=0.45;duckRate=8",
      "0:0,0,0.45;1,1,0|15:0,0.38,0.5;1,1,0|24:0,0.42,0.2;1,0.85,0" },

    { "Scale Riser", "Whole Tone Drift",
      "scaleOn=1;scaleKey=0;scaleType=11;"
      "osc1Wave=0;osc1Uni=5;osc1Det=18;osc1KeyStart=48;osc1KeyEnd=96;"
      "osc3On=1;osc3Wave=1;osc3Coarse=-12;osc3Level=0.4;osc3KeyStart=48;osc3KeyEnd=96;"
      "bars=9;release=1400;flt1On=0;"
      "fx1Type=4;revAmt=0.68;revShimmer=0.8;revDecay=0.9;fx2Type=2;choAmt=0.5;choDepth=0.65",
      "0:0,0,0.65;1,1,0|8:0,0,0.65;1,1,0|30:0,0.5,0.3;1,0.95,0|31:0,0.4,0.3;1,0.9,0" },

    { "Scale Riser", "Octave Jump Build",
      "scaleOn=1;scaleKey=0;scaleType=69;"
      "osc1Wave=2;osc1Uni=1;osc1Level=0.9;osc1KeyStart=36;osc1KeyEnd=96;attack=0.5;release=180;"
      "bars=8;flt1Cutoff=1500;flt1Res=1.5;flt1Env=0.6;"
      "fx1Type=5;duckAmt=0.7;duckRate=11;duckShape=5;fx2Type=3;dlyAmt=0.4;dlyTime=8;dlyFb=0.55",
      "0:0,0,0.25;1,1,0|15:0,0.5,0.3;1,1,0|33:0,0.5,0;1,0.15,0" },

    { "Scale Riser", "Fifths Power Lift",
      "scaleOn=1;scaleKey=0;scaleType=12;"
      "osc1Wave=3;osc1Uni=7;osc1Det=35;osc1Spread=1;osc1KeyStart=33;osc1KeyEnd=81;"
      "osc2On=1;osc2Wave=3;osc2Coarse=12;osc2Uni=5;osc2Det=20;osc2Level=0.5;"
      "osc2KeyStart=33;osc2KeyEnd=81;"
      "flt1Cutoff=500;flt1Env=0.88;flt1Res=2;bars=8;"
      "fx1Type=1;satAmt=0.45;satDrive=4;satAlgo=4;fx2Type=4;revAmt=0.4",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|15:0,0.4,0.5;1,1,0|30:0,0.42,0.25;1,0.88,0" },

    { "Scale Riser", "Minor 7th Arp Riser",
      "scaleOn=1;scaleKey=9;scaleType=65;"
      "osc1Wave=2;osc1Uni=3;osc1Det=12;osc1KeyStart=45;osc1KeyEnd=93;attack=0.8;release=220;"
      "bars=8;flt1Cutoff=1100;flt1Res=3;flt1Env=0.65;"
      "fx1Type=3;dlyAmt=0.5;dlyTime=8;dlyFb=0.62;dlyDuck=0.55;fx2Type=4;revAmt=0.32;fx3Type=5;duckAmt=0.5;duckRate=11",
      "0:0,0,0.15;1,1,0|15:0,0.5,0.3;1,1,0|27:0,0.4,0.3;1,0.92,0" },

    { "Scale Riser", "Ryukyu Sunrise",
      "scaleOn=1;scaleKey=0;scaleType=50;"
      "osc1Wave=1;osc1Uni=5;osc1Det=20;osc1Spread=0.85;osc1KeyStart=48;osc1KeyEnd=84;"
      "noiseType=1;noiseLevel=0.28;noisePitch=3500;noiseRange=3;"
      "bars=9;release=1200;flt1Cutoff=1600;flt1Env=0.5;"
      "fx1Type=2;choAmt=0.45;choDepth=0.55;fx2Type=4;revAmt=0.62;revShimmer=0.7;revDecay=0.88",
      "0:0,0,0.6;1,1,0|13:0,0.28,0.5;1,0.88,0|15:0,0.5,0.3;1,1,0|30:0,0.45,0.3;1,0.95,0" },

    { "Scale Riser", "Chromatic Stairs Down",
      "scaleOn=1;scaleKey=0;scaleType=0;"
      "osc1Wave=3;osc1Uni=5;osc1Det=26;osc1KeyStart=88;osc1KeyEnd=40;"
      "bars=6;release=500;flt1Cutoff=3500;flt1Env=-0.75;"
      "fx1Type=1;satAmt=0.4;satDrive=4;satAlgo=3;fx2Type=4;revAmt=0.42;revDecay=0.8",
      "0:0,0,0.4;1,1,0|15:0,0.5,0.4;1,1,0" },

    // ================= EDM =================
    { "EDM", "Future Bass Lift",
      "osc1Wave=3;osc1Uni=7;osc1Det=48;osc1Spread=1;osc1KeyStart=48;osc1KeyEnd=84;"
      "osc2On=1;osc2Wave=1;osc2Coarse=12;osc2Level=0.4;osc2Uni=3;osc2Det=20;"
      "osc2KeyStart=48;osc2KeyEnd=84;"
      "flt1Cutoff=800;flt1Res=1.2;flt1Env=0.75;bars=7;release=420;"
      "fx1Type=2;choAmt=0.55;choDepth=0.7;fx2Type=4;revAmt=0.42;revShimmer=0.45;fx3Type=5;duckAmt=0.5;duckRate=5",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|25:0,0.4,0.3;1,0.9,0" },

    { "EDM", "Snare Roll Noise",
      "osc1On=0;noiseType=0;noiseLevel=0.85;noisePitch=2200;noiseRes=1.5;noiseRange=3.5;"
      "flt1Type=1;flt1Cutoff=300;flt1Env=0.6;bars=6;release=250;"
      "fx1Type=5;duckAmt=0.9;duckRate=11;duckShape=6;fx2Type=1;satAmt=0.35;satDrive=3;fx3Type=4;revAmt=0.28",
      "12:0,0.42,0.35;1,0.92,0|13:0,0.3,0.45;1,1,0|33:0,0.5,0;0.6,0.28,0;1,0.02,0|15:0,0.45,0.4;1,1,0" },

    { "EDM", "Tunnel Sweep",
      "osc1Wave=3;osc1Uni=7;osc1Det=55;osc1Spread=1;osc1KeyStart=36;osc1KeyEnd=72;"
      "noiseType=1;noiseLevel=0.45;noisePitch=900;noiseRes=2.5;noiseRange=5;"
      "flt1Type=2;flt1Cutoff=600;flt1Res=4.5;flt1Env=0.9;bars=8;release=550;"
      "fx1Type=1;satAmt=0.4;satDrive=4.5;fx2Type=4;revAmt=0.5;revDecay=0.82;revDamp=0.45",
      "0:0,0,0.45;1,1,0|12:0,0.4,0.4;1,0.95,0|13:0,0.3,0.4;1,0.9,0|15:0,0.38,0.5;1,1,0" },

    { "EDM", "Reverse Impact Build",
      "osc1Wave=3;osc1Uni=5;osc1Det=30;osc1KeyStart=40;osc1KeyEnd=88;attack=400;release=60;"
      "noiseType=1;noiseLevel=0.5;noiseRange=5;bars=7;"
      "flt1Cutoff=500;flt1Env=0.85;fx1Type=4;revAmt=0.5;revDecay=0.85;fx2Type=1;satAmt=0.35;satDrive=3.5",
      "0:0,0,0.6;1,1,0|1:0,0.15,0.7;1,1,0|13:0,0.1,0.8;1,1,0|15:0,0.35,0.6;1,1,0" },

    // ================= Trance =================
    { "Trance", "Psy Lift",
      "osc1Wave=3;osc1Uni=5;osc1Det=28;osc1KeyStart=36;osc1KeyEnd=84;"
      "flt1Cutoff=350;flt1Res=7;flt1Env=0.95;bars=8;release=350;"
      "fx1Type=1;satAmt=0.5;satDrive=6;satAlgo=5;fx2Type=5;duckAmt=0.75;duckRate=8;duckShape=4;fx3Type=4;revAmt=0.35",
      "0:0,0,0.45;1,1,0|15:0,0.35,0;0.2,0.62,-0.4;0.4,0.5,0;0.6,0.78,-0.4;0.8,0.68,0;1,1,0" },

    { "Trance", "Air Layer Rise",
      "osc1Wave=0;osc1Uni=5;osc1Det=14;osc1Level=0.6;osc1KeyStart=60;osc1KeyEnd=100;"
      "noiseType=1;noiseLevel=0.45;noisePitch=4000;noiseRes=1.2;noiseRange=3;"
      "bars=9;release=1100;flt1On=0;"
      "fx1Type=2;choAmt=0.5;choWidth=1;fx2Type=4;revAmt=0.65;revShimmer=0.75;revDecay=0.88",
      "0:0,0,0.6;1,1,0|12:0,0.45,0.4;1,0.95,0|13:0,0.25,0.5;1,0.9,0|30:0,0.5,0.3;1,0.95,0" },

    { "Trance", "Rolling Gate Lift",
      "osc1Wave=3;osc1Uni=7;osc1Det=35;osc1Spread=1;osc1KeyStart=45;osc1KeyEnd=81;"
      "flt1Cutoff=800;flt1Res=2;flt1Env=0.8;bars=8;release=300;"
      "fx1Type=5;duckAmt=0.85;duckRate=8;duckShape=3;fx2Type=3;dlyAmt=0.35;dlyTime=5;dlyFb=0.5;fx3Type=4;revAmt=0.35",
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|33:0,0.5,0;0.5,0.5,0;0.5,0.25,0;1,0.25,0" },

    // ================= Techno =================
    { "Techno", "Modular Bleep Climb",
      "scaleOn=1;scaleKey=0;scaleType=13;"
      "osc1Wave=2;osc1Uni=1;osc1Level=0.85;osc1KeyStart=48;osc1KeyEnd=96;attack=0.3;release=90;"
      "bars=8;flt1Cutoff=1200;flt1Res=5;flt1Env=0.7;"
      "fx1Type=3;dlyAmt=0.5;dlyTime=8;dlyFb=0.68;dlyDamp=0.4;fx2Type=5;duckAmt=0.6;duckRate=11",
      "0:0,0,0.2;1,1,0|15:0,0.45,0.4;1,1,0|27:0,0.4,0.3;1,0.9,0" },

    { "Techno", "Sub Drone Swell",
      "osc1Wave=0;osc1Level=0.95;osc1KeyStart=24;osc1KeyEnd=40;"
      "osc2On=1;osc2Wave=3;osc2Level=0.35;osc2Coarse=12;osc2Uni=3;osc2Det=18;"
      "osc2KeyStart=24;osc2KeyEnd=40;"
      "noiseType=2;noiseLevel=0.4;noisePitch=90;noiseRange=2;"
      "flt1Cutoff=400;flt1Env=0.7;bars=9;attack=150;release=1200;"
      "fx1Type=1;satAmt=0.45;satDrive=4;satAlgo=4;fx2Type=4;revAmt=0.35;revDamp=0.6",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0|1:0,0.2,0.6;1,1,0|13:0,0.3,0.4;1,0.85,0|15:0,0.42,0.4;1,1,0" },

    { "Techno", "Metallic Comb Rise",
      "osc1Wave=4;osc1Uni=5;osc1Det=45;osc1KeyStart=44;osc1KeyEnd=92;"
      "flt1Type=3;flt1Cutoff=700;flt1Res=8;flt1Env=0.9;"
      "flt2On=1;flt2Type=3;flt2Cutoff=2400;flt2Res=7;flt2Env=0.7;bars=8;"
      "fx1Type=1;satAmt=0.5;satDrive=6;satAlgo=7;fx2Type=4;revAmt=0.4;revDamp=0.5",
      "0:0,0,0.5;1,1,0|15:0,0.4,0.4;1,1,0|16:0,0.45,0.4;1,1,0|23:0,0.4,0.2;1,0.85,0" },

    // ================= Dubstep =================
    { "Dubstep", "Wobble Riser",
      "osc1Wave=3;osc1Uni=5;osc1Det=38;osc1KeyStart=28;osc1KeyEnd=64;"
      "flt1Cutoff=300;flt1Res=9;flt1Env=0.95;bars=7;release=300;"
      "fx1Type=1;satAmt=0.6;satDrive=7;satAlgo=6;fx2Type=5;duckAmt=0.6;duckRate=8;duckShape=3",
      "0:0,0,0.4;1,1,0|15:0,0.3,0;0.15,0.8,-0.5;0.3,0.35,0;0.45,0.85,-0.5;0.6,0.4,0;0.8,0.92,-0.5;1,1,0" },

    { "Dubstep", "Screech Uplift",
      "osc1Wave=4;osc1Uni=7;osc1Det=52;osc1Spread=1;osc1KeyStart=52;osc1KeyEnd=100;"
      "flt1Type=1;flt1Cutoff=180;flt1Env=0.55;bars=6;release=250;"
      "fx1Type=1;satAmt=0.6;satDrive=8;satAlgo=7;fx2Type=2;choAmt=0.35;fx3Type=4;revAmt=0.3",
      "0:0,0,0.6;1,1,0|2:0,0.5,0.4;1,0.9,0|15:0,0.5,0.3;1,1,0|23:0,0.42,0.2;1,0.9,0" },

    { "Dubstep", "Sub Drop Charge",
      "osc1Wave=0;osc1Level=1;osc1KeyStart=36;osc1KeyEnd=24;"
      "osc2On=1;osc2Wave=4;osc2Level=0.3;osc2KeyStart=48;osc2KeyEnd=84;"
      "noiseType=2;noiseLevel=0.35;noisePitch=140;noiseRange=2;"
      "flt1On=0;bars=6;release=900;fx1Type=1;satAmt=0.5;satDrive=5;satAlgo=3",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|1:0,0.25,0.5;1,1,0|13:0,0.35,0.3;1,0.8,0" },

    { "Dubstep", "Grit Noise Build",
      "osc1On=0;noiseType=0;noiseLevel=1;noisePitch=400;noiseRes=1;noiseRange=6;"
      "flt1Type=1;flt1Cutoff=150;flt1Env=0.75;bars=7;"
      "fx1Type=1;satAmt=0.65;satDrive=8;satAlgo=1;fx2Type=5;duckAmt=0.7;duckRate=11;duckShape=5;fx3Type=4;revAmt=0.3",
      "12:0,0.35,0.5;1,1,0|13:0,0.3,0.5;1,1,0|15:0,0.4,0.5;1,1,0|23:0,0.4,0.2;1,0.85,0" },

    // ================= DnB =================
    { "DnB", "Amen Sweep Up",
      "osc1On=0;noiseType=0;noiseLevel=0.9;noisePitch=1800;noiseRes=2;noiseRange=4;"
      "flt1On=0;bars=6;release=180;"
      "fx1Type=5;duckAmt=0.85;duckRate=14;duckShape=6;fx2Type=1;satAmt=0.4;satDrive=4;fx3Type=3;dlyAmt=0.3;dlyTime=8;dlyFb=0.5",
      "12:0,0.4,0.4;1,0.95,0|13:0,0.25,0.5;1,1,0|33:0,0.5,0;1,0.1,0" },

    { "DnB", "Neuro Ramp",
      "scaleOn=1;scaleKey=5;scaleType=4;"
      "osc1Wave=4;osc1Uni=5;osc1Det=42;osc1KeyStart=29;osc1KeyEnd=77;"
      "flt1Type=2;flt1Cutoff=500;flt1Res=8;flt1Env=0.92;"
      "flt2On=1;flt2Cutoff=4000;flt2Res=2;flt2Env=0.5;bars=7;"
      "fx1Type=1;satAmt=0.6;satDrive=7;satAlgo=6;fx2Type=4;revAmt=0.28",
      "0:0,0,0.3;1,1,0|15:0,0.35,0.4;1,1,0|16:0,0.45,0.4;1,1,0" },

    { "DnB", "Reese Tension",
      "osc1Wave=3;osc1Uni=7;osc1Det=65;osc1Spread=1;osc1KeyStart=26;osc1KeyEnd=50;"
      "flt1Cutoff=450;flt1Res=3;flt1Env=0.8;bars=8;release=400;"
      "fx1Type=1;satAmt=0.5;satDrive=5;satAlgo=4;fx2Type=2;choAmt=0.4;choRate=0.3;fx3Type=4;revAmt=0.3",
      "0:0,0,0.4;1,1,0|2:0,0.45,0.3;1,0.85,0|15:0,0.4,0.4;1,1,0" },

    // ================= Hardstyle =================
    { "Hardstyle", "Screech Riser",
      "osc1Wave=2;osc1Uni=7;osc1Det=45;osc1Spread=1;osc1KeyStart=48;osc1KeyEnd=96;"
      "flt1Type=1;flt1Cutoff=250;flt1Env=0.6;bars=7;release=200;"
      "fx1Type=1;satAmt=0.7;satDrive=9;satAlgo=1;fx2Type=5;duckAmt=0.8;duckRate=8;duckShape=6;fx3Type=4;revAmt=0.3",
      "0:0,0,0.55;1,1,0|15:0,0.5,0.3;1,1,0|23:0,0.45,0.2;1,0.9,0" },

    { "Hardstyle", "Euphoric Uplift",
      "scaleOn=1;scaleKey=7;scaleType=2;"
      "osc1Wave=3;osc1Uni=7;osc1Det=38;osc1Spread=1;osc1KeyStart=43;osc1KeyEnd=91;"
      "osc2On=1;osc2Wave=3;osc2Coarse=-12;osc2Uni=5;osc2Det=22;osc2Level=0.5;"
      "osc2KeyStart=43;osc2KeyEnd=91;"
      "flt1Cutoff=600;flt1Res=2;flt1Env=0.88;bars=8;release=700;"
      "fx1Type=1;satAmt=0.45;satDrive=5;satAlgo=4;fx2Type=4;revAmt=0.5;revShimmer=0.55;revDecay=0.85",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|15:0,0.4,0.5;1,1,0|30:0,0.45,0.3;1,0.92,0" },

    // ================= Ambient =================
    { "Ambient", "Slow Bloom",
      "osc1Wave=0;osc1Uni=7;osc1Det=12;osc1Level=0.7;osc1KeyStart=48;osc1KeyEnd=72;"
      "osc3On=1;osc3Wave=1;osc3Coarse=7;osc3Level=0.4;osc3KeyStart=48;osc3KeyEnd=72;"
      "attack=350;release=2500;bars=9;flt1Cutoff=1200;flt1Env=0.5;flt1Res=0.8;"
      "fx1Type=2;choAmt=0.5;choRate=0.15;fx2Type=4;revAmt=0.75;revShimmer=0.85;revDecay=0.92;revMod=0.6",
      "0:0,0,0.7;1,1,0|8:0,0,0.7;1,1,0|15:0,0.5,0.4;1,1,0|30:0,0.55,0.3;1,0.95,0" },

    { "Ambient", "Granular Air",
      "osc1On=0;noiseType=1;noiseLevel=0.6;noisePitch=1800;noiseRes=4;noiseRange=4;"
      "flt1On=0;bars=9;attack=300;release=2000;"
      "fx1Type=2;choAmt=0.45;choRate=0.2;fx2Type=3;dlyAmt=0.35;dlyTime=0;dlyFb=0.7;dlyDamp=0.6;"
      "fx3Type=4;revAmt=0.7;revShimmer=0.8;revDecay=0.9",
      "12:0,0.4,0.5;1,0.9,0|13:0,0.25,0.6;1,0.85,0|14:0,0.45,0.4;1,0.85,0|30:0,0.5,0.3;1,0.95,0" },

    { "Ambient", "Quartal Drift",
      "scaleOn=1;scaleKey=2;scaleType=13;"
      "osc1Wave=1;osc1Uni=5;osc1Det=10;osc1Level=0.65;osc1KeyStart=52;osc1KeyEnd=88;"
      "attack=200;release=2200;bars=9;flt1Cutoff=1800;flt1Env=0.4;"
      "fx1Type=2;choAmt=0.5;choRate=0.12;fx2Type=4;revAmt=0.72;revShimmer=0.8;revDecay=0.9;revMod=0.55",
      "0:0,0,0.6;1,1,0|15:0,0.5,0.3;1,1,0|30:0,0.52,0.3;1,0.95,0" },

    { "Ambient", "Deep Space Swell",
      "osc1Wave=0;osc1Level=0.8;osc1KeyStart=28;osc1KeyEnd=52;"
      "osc2On=1;osc2Wave=0;osc2Coarse=12;osc2Level=0.35;osc2Uni=3;osc2Det=8;"
      "osc2KeyStart=28;osc2KeyEnd=52;"
      "noiseType=2;noiseLevel=0.3;noisePitch=200;noiseRange=3;"
      "attack=450;release=3000;bars=9;flt1Cutoff=700;flt1Env=0.55;"
      "fx1Type=4;revAmt=0.8;revDecay=0.94;revDamp=0.55;revMod=0.5",
      "0:0,0,0.7;1,1,0|4:0,0,0.7;1,1,0|13:0,0.3,0.5;1,0.8,0|15:0,0.5,0.4;1,1,0" },

    // ================= Cinematic =================
    { "Cinematic", "Horror Cluster Rise",
      "scaleOn=1;scaleKey=0;scaleType=15;"
      "osc1Wave=3;osc1Uni=7;osc1Det=58;osc1Spread=1;osc1KeyStart=32;osc1KeyEnd=80;"
      "osc2On=1;osc2Wave=1;osc2Coarse=6;osc2Uni=5;osc2Det=40;osc2Level=0.45;"
      "osc2KeyStart=32;osc2KeyEnd=80;"
      "flt1Cutoff=500;flt1Res=3;flt1Env=0.85;bars=9;release=1500;"
      "fx1Type=1;satAmt=0.4;satDrive=4;fx2Type=4;revAmt=0.6;revDecay=0.9;revDamp=0.5",
      "0:0,0,0.6;1,1,0|4:0,0,0.6;1,1,0|15:0,0.4,0.55;1,1,0|30:0,0.45,0.35;1,0.95,0" },

    { "Cinematic", "War Drum Riser",
      "osc1Wave=0;osc1Level=0.85;osc1KeyStart=24;osc1KeyEnd=45;"
      "noiseType=2;noiseLevel=0.6;noisePitch=180;noiseRes=1.2;noiseRange=3;"
      "flt1Cutoff=350;flt1Env=0.7;bars=8;attack=80;release=1000;"
      "fx1Type=5;duckAmt=0.75;duckRate=5;duckShape=4;fx2Type=1;satAmt=0.5;satDrive=5;satAlgo=4;fx3Type=4;revAmt=0.45",
      "0:0,0,0.45;1,1,0|13:0,0.35,0.4;1,0.9,0|15:0,0.42,0.4;1,1,0|33:0,0.5,0;1,0.2,0" },

    { "Cinematic", "Whoosh Transition",
      "osc1On=0;noiseType=0;noiseLevel=0.85;noisePitch=800;noiseRes=1.8;noiseRange=6;"
      "flt1On=0;bars=5;attack=120;release=800;"
      "fx1Type=2;choAmt=0.5;choWidth=1;fx2Type=4;revAmt=0.55;revDecay=0.85",
      "12:0,0.15,0;0.5,0.9,0;1,0.2,0|13:0,0.1,0.5;0.5,1,0;1,0.15,0" },

    // ================= Downer =================
    { "Downer", "Scale Fall",
      "scaleOn=1;scaleKey=9;scaleType=4;"
      "osc1Wave=3;osc1Uni=5;osc1Det=28;osc1KeyStart=93;osc1KeyEnd=45;"
      "bars=6;release=600;flt1Cutoff=3000;flt1Env=-0.7;"
      "fx1Type=3;dlyAmt=0.4;dlyTime=8;dlyFb=0.6;fx2Type=4;revAmt=0.45;revDecay=0.82",
      "0:0,0,0.35;1,1,0|15:0,0.5,0.4;1,1,0|27:0,0.4,0.3;1,0.9,0" },

    { "Downer", "Vacuum Suck",
      "osc1On=0;noiseType=1;noiseLevel=0.9;noisePitch=8000;noiseRes=3;noiseRange=7;"
      "flt1On=0;bars=4;attack=60;release=500;"
      "fx1Type=1;satAmt=0.35;satDrive=3;fx2Type=4;revAmt=0.5;revDecay=0.85;revDamp=0.4",
      "12:0,0.95,-0.5;1,0.05,0|13:0,0.85,0.3;1,0.25,0|14:0,0.4,0;1,0.85,0" },

    // ---- EDM (v0.6 追加: 新機能ショーケース) ----
    { "EDM", "Stutter Gate Build",
      "osc1Wave=3;osc1Uni=7;osc1Det=35;osc1Spread=1;osc1KeyStart=36;osc1KeyEnd=93;"
      "flt1Cutoff=500;flt1Env=0.85;flt1Res=1.4;bars=7;release=300;fx1Type=6;"
      "stutAmt=0.85;stutRate=11;fx2Type=4;revAmt=0.35",
      "0:0,0,0.5;1,1,0|15:0,0.4,0.45;1,1,0|39:0,0.15,0.6;1,0.95,0|40:0,0.5,0;1,0.05,0" },
    { "EDM", "Vocal Sweep Lift",
      "osc1Wave=3;osc1Uni=5;osc1Det=28;osc1KeyStart=40;osc1KeyEnd=88;flt1Type=4;"
      "flt1Cutoff=140;flt1Res=6;flt1Env=0.9;bars=7;release=420;fx1Type=1;satAmt=0.3;"
      "satDrive=3;fx2Type=4;revAmt=0.4;revShimmer=0.35",
      "0:0,0,0.5;1,1,0|15:0,0.1,0.3;0.5,0.6,0;1,1,0|30:0,0.42,0.3;1,0.9,0" },
    { "EDM", "Wide Pan Riser",
      "osc1Wave=3;osc1Uni=5;osc1Det=30;osc1Pan=-0.5;osc1KeyStart=38;osc1KeyEnd=90;"
      "osc2On=1;osc2Wave=3;osc2Uni=5;osc2Det=30;osc2Pan=0.5;osc2Coarse=7;"
      "osc2Level=0.6;osc2KeyStart=38;osc2KeyEnd=90;flt1Cutoff=600;flt1Env=0.8;bars=7;"
      "fx1Type=4;revAmt=0.4;fx2Type=5;duckAmt=0.45;duckRate=8",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|35:0,0.15,0;1,0.9,0|36:0,0.85,0;1,0.1,0" },

    // ---- Trance (v0.6 追加: 新機能ショーケース) ----
    { "Trance", "Acid Comb Climb",
      "osc1Wave=3;osc1KeyStart=36;osc1KeyEnd=76;flt1Type=5;flt1Cutoff=220;flt1Res=9;"
      "flt1Env=0.9;bars=7;release=380;fx1Type=1;satAmt=0.45;satDrive=4;satAlgo=2;"
      "fx2Type=3;dlyAmt=0.3;dlyTime=8;dlyFb=0.5",
      "0:0,0,0.4;1,1,0|15:0,0.2,0.35;1,0.95,0|27:0,0.35,0.3;1,0.9,0" },
    { "Trance", "Stereo Supersaw Rise",
      "osc1Wave=3;osc1Uni=7;osc1Det=50;osc1Spread=1;osc1Pan=-0.35;osc1KeyStart=45;"
      "osc1KeyEnd=88;osc2On=1;osc2Wave=3;osc2Uni=7;osc2Det=40;osc2Spread=1;"
      "osc2Pan=0.35;osc2Fine=8;osc2Level=0.6;osc2KeyStart=45;osc2KeyEnd=88;"
      "flt1Cutoff=700;flt1Env=0.85;bars=8;release=650;fx1Type=2;choAmt=0.45;fx2Type=4;"
      "revAmt=0.5;revShimmer=0.5",
      "0:0,0,0.6;1,1,0|4:0,0,0.6;1,1,0|15:0,0.42,0.5;1,1,0|35:0,0.3,0;1,0.05,0|36:0,0.7,0;1,0.95,0" },
    { "Trance", "Gate Accel Trance",
      "osc1Wave=3;osc1Uni=7;osc1Det=30;osc1Spread=1;osc1KeyStart=45;osc1KeyEnd=93;"
      "flt1Cutoff=800;flt1Env=0.7;bars=8;release=500;fx1Type=4;revAmt=0.42",
      "0:0,0,0.55;1,1,0|1:R16;0,0.9,0;0.45,0.9,0;0.5,0.1,0;1,0.1,0|15:0,0.45,0.5;1,1,0" },
    { "Trance", "Humanized Uplifter",
      "osc1Wave=3;osc1Uni=7;osc1Det=45;osc1Spread=1;osc1KeyStart=45;osc1KeyEnd=93;"
      "osc3On=1;osc3Wave=0;osc3Coarse=-12;osc3Level=0.45;osc3KeyStart=45;"
      "osc3KeyEnd=93;noiseType=1;noiseLevel=0.3;humanize=0.55;bars=9;release=900;"
      "fx1Type=4;revAmt=0.5;revShimmer=0.65;revDecay=0.85",
      "0:0,0,0.7;1,1,0|8:0,0,0.7;1,1,0|13:0,0.3,0.6;1,0.9,0|15:0,0.42,0.6;1,1,0|30:0,0.42,0.4;1,0.95,0" },

    // ---- Bass (v0.6 追加: 新機能ショーケース) ----
    { "Bass", "Vowel Growl",
      "osc1Wave=4;osc1KeyStart=26;osc1KeyEnd=62;flt1Type=4;flt1Cutoff=160;flt1Res=8;"
      "flt1Env=0.95;bars=6;fx1Type=1;satAmt=0.6;satDrive=6;satAlgo=6",
      "0:0,0,0.35;1,1,0|15:0,0.05,0;0.35,0.75,0;0.7,0.25,0;1,0.95,0|23:0,0.4,0;1,0.85,0" },
    { "Bass", "Comb Resonator Bass",
      "osc1On=0;noiseType=2;noiseLevel=0.85;noisePitch=200;noiseRange=3;flt1Type=5;"
      "flt1Cutoff=60;flt1Res=10;flt1Env=0.9;bars=6;fx1Type=1;satAmt=0.4;satDrive=4",
      "13:0,0.35,0.3;1,0.9,0|15:0,0.15,0.4;1,0.9,0" },
    { "Bass", "Stutter Bass Charge",
      "osc1Wave=2;osc1Uni=3;osc1Det=18;osc1KeyStart=28;osc1KeyEnd=64;flt1Cutoff=350;"
      "flt1Res=4;flt1Env=0.85;bars=6;fx1Type=6;stutAmt=0.8;stutRate=8;fx2Type=1;"
      "satAmt=0.45;satDrive=5",
      "0:0,0,0.4;1,1,0|15:0,0.35,0.35;1,1,0|39:0,0.1,0.5;1,0.9,0|40:0,0.5,0;1,0.1,0" },
    { "Bass", "Wide Reese Rise",
      "osc1Wave=3;osc1Uni=3;osc1Det=22;osc1Pan=-0.45;osc1KeyStart=26;osc1KeyEnd=58;"
      "osc2On=1;osc2Wave=3;osc2Uni=3;osc2Det=22;osc2Pan=0.45;osc2Fine=-14;"
      "osc2Level=0.8;osc2KeyStart=26;osc2KeyEnd=58;flt1Cutoff=300;flt1Res=3;"
      "flt1Env=0.85;bars=6;fx1Type=1;satAmt=0.4;satDrive=4",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0|15:0,0.35,0.4;1,1,0|35:0,0.25,0;1,0.1,0|36:0,0.75,0;1,0.9,0" },
    { "Bass", "Detuned Sub Climb",
      "osc1Wave=0;osc1Level=0.95;osc1KeyStart=24;osc1KeyEnd=48;osc2On=1;osc2Wave=0;"
      "osc2Fine=12;osc2Level=0.55;osc2KeyStart=24;osc2KeyEnd=48;flt1On=0;bars=5;"
      "fx1Type=5;duckAmt=0.5;duckRate=5",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0" },
    { "Bass", "Talkbox Ramp",
      "osc1Wave=3;osc1Uni=3;osc1Det=14;osc1KeyStart=30;osc1KeyEnd=66;scaleOn=1;"
      "scaleKey=0;scaleType=4;flt1Type=4;flt1Cutoff=200;flt1Res=7;flt1Env=0.9;bars=6;"
      "fx1Type=1;satAmt=0.35;satDrive=3",
      "0:0,0,0;0.5,0.5,0;1,1,0|15:0,0.1,0;0.5,0.9,0;1,0.35,0" },
    { "Bass", "Neuro Comb Wobble",
      "osc1Wave=4;osc1Uni=3;osc1Det=20;osc1KeyStart=26;osc1KeyEnd=60;flt1Type=5;"
      "flt1Cutoff=90;flt1Res=8;flt1Env=0.85;bars=6;fx1Type=1;satAmt=0.55;satDrive=6;"
      "satAlgo=7",
      "0:0,0,0.4;1,1,0|15:R8;0,0.2,0;0.5,0.9,0;1,0.2,0|23:0,0.4,0;1,0.9,0" },

    // ---- Techno (v0.6 追加: 新機能ショーケース) ----
    { "Techno", "Comb Tunnel",
      "osc1On=0;noiseType=1;noiseLevel=0.9;noisePitch=400;noiseRange=4;flt1Type=5;"
      "flt1Cutoff=110;flt1Res=9;flt1Env=0.85;bars=7;fx1Type=3;dlyAmt=0.35;dlyTime=5;"
      "dlyFb=0.6;fx2Type=4;revAmt=0.35",
      "12:0,0.4,0.3;1,0.9,0|13:0,0.3,0.4;1,0.95,0|15:0,0.15,0.35;1,0.9,0" },
    { "Techno", "Stutter Loop Riser",
      "osc1Wave=2;osc1Uni=3;osc1Det=12;osc1KeyStart=36;osc1KeyEnd=72;flt1Cutoff=700;"
      "flt1Env=0.7;bars=7;fx1Type=6;stutAmt=0.9;stutRate=8;fx2Type=5;duckAmt=0.5;"
      "duckRate=5",
      "0:0,0,0.4;1,1,0|15:0,0.45,0.35;1,1,0|40:0,0.5,0;0.5,0.5,0;1,0.1,0" },
    { "Techno", "Vowel Machine",
      "osc1Wave=3;osc1Uni=3;osc1Det=16;osc1KeyStart=34;osc1KeyEnd=70;flt1Type=4;"
      "flt1Cutoff=180;flt1Res=9;flt1Env=0.95;bars=7;fx1Type=1;satAmt=0.4;satDrive=4;"
      "satAlgo=4;fx2Type=5;duckAmt=0.55;duckRate=8",
      "0:0,0,0.45;1,1,0|15:R4;0,0.1,0;0.5,0.9,0;1,0.1,0" },
    { "Techno", "Panning Rumble",
      "osc1Wave=0;osc1Level=0.9;osc1Pan=-0.4;osc1KeyStart=24;osc1KeyEnd=52;"
      "noiseType=2;noiseLevel=0.5;noisePan=0.4;noisePitch=180;noiseRange=3;"
      "flt1Cutoff=400;flt1Env=0.7;bars=8;fx1Type=4;revAmt=0.4;revDecay=0.8",
      "0:0,0,0.5;1,1,0|13:0,0.3,0.4;1,0.9,0|15:0,0.4,0.4;1,1,0|35:0,0.3,0;1,0.7,0|38:0,0.7,0;1,0.3,0" },

    // ---- Cinematic (v0.6 追加: 新機能ショーケース) ----
    { "Cinematic", "Choir Vowel Swell",
      "osc1Wave=1;osc1Uni=5;osc1Det=25;osc1Spread=1;osc1KeyStart=40;osc1KeyEnd=76;"
      "osc2On=1;osc2Wave=1;osc2Coarse=7;osc2Uni=5;osc2Det=20;osc2Level=0.55;"
      "osc2KeyStart=40;osc2KeyEnd=76;flt1Type=4;flt1Cutoff=200;flt1Res=5;flt1Env=0.8;"
      "bars=9;attack=120;release=1500;humanize=0.4;fx1Type=4;revAmt=0.6;revDecay=0.9;"
      "revShimmer=0.5",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0|15:0,0.15,0.3;1,0.85,0|30:0,0.45,0.3;1,0.95,0" },
    { "Cinematic", "Metallic Resonance Rise",
      "osc1On=0;noiseType=0;noiseLevel=0.8;noisePitch=600;noiseRange=5;flt1Type=5;"
      "flt1Cutoff=140;flt1Res=11;flt1Env=0.9;bars=8;release=1200;fx1Type=4;"
      "revAmt=0.55;revShimmer=0.7;revDecay=0.88",
      "12:0,0.4,0.3;1,0.95,0|13:0,0.25,0.4;1,0.9,0|15:0,0.15,0.4;1,0.9,0|30:0,0.4,0.3;1,0.95,0" },
    { "Cinematic", "Panning Whoosh",
      "osc1On=0;noiseType=1;noiseLevel=0.9;noisePitch=300;noiseRes=2;noiseRange=6;"
      "noisePan=-0.8;flt1On=0;bars=6;release=800;fx1Type=4;revAmt=0.5;revDecay=0.85",
      "12:0,0.3,0.4;1,1,0|13:0,0.2,0.4;0.7,1,0;1,0.5,0|38:0,0.05,0;1,0.95,0" },
    { "Cinematic", "Breathing Drone",
      "osc1Wave=0;osc1Uni=3;osc1Det=10;osc1KeyStart=28;osc1KeyEnd=52;osc2On=1;"
      "osc2Wave=1;osc2Coarse=12;osc2Level=0.4;osc2KeyStart=28;osc2KeyEnd=52;"
      "flt1Cutoff=500;flt1Env=0.6;bars=9;attack=200;release=2000;humanize=0.8;"
      "fx1Type=4;revAmt=0.65;revDecay=0.92;revMod=0.6",
      "0:0,0,0.3;1,1,0|4:0,0,0.3;1,1,0|1:R3;0,0.35,0;0.5,0.65,0;1,0.35,0|15:0,0.35,0.3;1,0.9,0" },

    // ---- Downer (v0.6 追加: 新機能ショーケース) ----
    { "Downer", "Vowel Descent",
      "osc1Wave=3;osc1Uni=5;osc1Det=25;osc1KeyStart=84;osc1KeyEnd=36;flt1Type=4;"
      "flt1Cutoff=3000;flt1Res=6;flt1Env=-0.85;bars=6;release=500;fx1Type=4;"
      "revAmt=0.4",
      "0:0,0,0.45;1,1,0|15:0,0.9,0;1,0.1,0" },
    { "Downer", "Stutter Fall",
      "osc1Wave=3;osc1Uni=3;osc1Det=20;osc1KeyStart=88;osc1KeyEnd=40;flt1Cutoff=2000;"
      "flt1Env=-0.7;bars=6;release=400;fx1Type=6;stutAmt=0.8;stutRate=11",
      "0:0,0,0.4;1,1,0|15:0,0.9,0;1,0.15,0|40:0,0.1,0;1,0.5,0" },
    { "Downer", "Comb Dive",
      "osc1On=0;noiseType=0;noiseLevel=0.85;noisePitch=2000;noiseRange=5;flt1Type=5;"
      "flt1Cutoff=800;flt1Res=9;flt1Env=-0.9;bars=5;release=450;fx1Type=4;revAmt=0.35",
      "12:0,0.9,0;1,0.1,0|13:0,0.9,0;1,0.35,0|15:0,0.9,0;1,0.1,0" },
    { "Downer", "Wide Collapse",
      "osc1Wave=3;osc1Uni=5;osc1Det=35;osc1Pan=-0.6;osc1KeyStart=86;osc1KeyEnd=34;"
      "osc2On=1;osc2Wave=3;osc2Uni=5;osc2Det=35;osc2Pan=0.6;osc2Level=0.7;"
      "osc2KeyStart=86;osc2KeyEnd=34;flt1Cutoff=2500;flt1Env=-0.8;bars=6;release=600;"
      "fx1Type=4;revAmt=0.45",
      "0:0,0,0.5;1,1,0|4:0,0,0.5;1,1,0|15:0,0.9,0;1,0.1,0|35:0,0.1,0;1,0.5,0|36:0,0.9,0;1,0.5,0" },
    { "Downer", "Humanized Drop",
      "osc1Wave=3;osc1Uni=7;osc1Det=40;osc1Spread=1;osc1KeyStart=90;osc1KeyEnd=33;"
      "noiseType=1;noiseLevel=0.35;humanize=0.6;flt1Cutoff=3000;flt1Env=-0.85;bars=7;"
      "release=700;fx1Type=4;revAmt=0.5;revDecay=0.85",
      "0:0,0,0.55;1,1,0|13:0,0.8,0;1,0.3,0|15:0,0.9,0;1,0.1,0" },

    // ---- Dubstep (v0.6 追加: 新機能ショーケース) ----
    { "Dubstep", "Vowel Wobble",
      "osc1Wave=3;osc1Uni=3;osc1Det=20;osc1KeyStart=28;osc1KeyEnd=64;flt1Type=4;"
      "flt1Cutoff=200;flt1Res=8;flt1Env=0.95;bars=6;fx1Type=1;satAmt=0.55;satDrive=6;"
      "satAlgo=6",
      "0:0,0,0.4;1,1,0|15:R6;0,0.1,0;0.5,0.9,0;1,0.1,0|23:0,0.4,0;1,0.9,0" },
    { "Dubstep", "Stutter Screech",
      "osc1Wave=4;osc1Uni=5;osc1Det=35;osc1KeyStart=48;osc1KeyEnd=96;flt1Type=2;"
      "flt1Cutoff=900;flt1Res=8;flt1Env=0.9;bars=6;fx1Type=6;stutAmt=0.85;stutRate=11;"
      "fx2Type=1;satAmt=0.5;satDrive=6;satAlgo=7",
      "0:0,0,0.5;1,1,0|15:0,0.35,0.4;1,1,0|40:0,0.5,0;1,0.05,0" },
    { "Dubstep", "Comb Growl Rise",
      "osc1Wave=4;osc1KeyStart=26;osc1KeyEnd=62;flt1Type=5;flt1Cutoff=80;flt1Res=10;"
      "flt1Env=0.9;bars=6;fx1Type=1;satAmt=0.6;satDrive=7;satAlgo=7",
      "0:0,0,0.4;1,1,0|15:0,0.15,0.4;1,0.9,0" },
    { "Dubstep", "Gate Chop Build",
      "osc1Wave=3;osc1Uni=5;osc1Det=30;osc1KeyStart=36;osc1KeyEnd=84;flt1Cutoff=600;"
      "flt1Env=0.8;bars=6;fx1Type=1;satAmt=0.4;satDrive=4",
      "0:0,0,0.5;1,1,0|1:R12;0,0.9,0;0.4,0.9,0;0.45,0.1,0;1,0.1,0|15:0,0.4,0.4;1,1,0" },
    { "Dubstep", "Wide Bass Riser",
      "osc1Wave=3;osc1Uni=5;osc1Det=28;osc1Pan=-0.5;osc1KeyStart=28;osc1KeyEnd=64;"
      "osc2On=1;osc2Wave=2;osc2Uni=3;osc2Det=20;osc2Pan=0.5;osc2Level=0.6;"
      "osc2KeyStart=28;osc2KeyEnd=64;flt1Cutoff=400;flt1Res=3;flt1Env=0.85;bars=6;"
      "fx1Type=1;satAmt=0.45;satDrive=5",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0|15:0,0.35,0.4;1,1,0|35:0,0.2,0;1,0.8,0|36:0,0.8,0;1,0.2,0" },
    { "Dubstep", "Talking Riser",
      "osc1Wave=3;osc1Uni=3;osc1Det=18;osc1KeyStart=34;osc1KeyEnd=70;scaleOn=1;"
      "scaleKey=2;scaleType=4;flt1Type=4;flt1Cutoff=250;flt1Res=9;flt1Env=0.9;bars=7;"
      "fx1Type=1;satAmt=0.4;satDrive=4",
      "0:0,0,0;0.33,0.35,0;0.66,0.7,0;1,1,0|15:0,0.05,0;0.3,0.8,0;0.6,0.2,0;1,0.95,0" },
    { "Dubstep", "Metallic Charge",
      "osc1On=0;noiseType=0;noiseLevel=0.9;noisePitch=900;noiseRange=4;flt1Type=5;"
      "flt1Cutoff=160;flt1Res=11;flt1Env=0.85;bars=6;fx1Type=1;satAmt=0.5;satDrive=5;"
      "fx2Type=4;revAmt=0.3",
      "12:0,0.4,0.3;1,0.9,0|13:0,0.3,0.4;1,0.95,0|15:0,0.2,0.4;1,0.9,0" },
    { "Dubstep", "Triplet Stutter Lift",
      "osc1Wave=2;osc1Uni=3;osc1Det=15;osc1KeyStart=40;osc1KeyEnd=82;flt1Cutoff=700;"
      "flt1Env=0.75;bars=6;fx1Type=6;stutAmt=0.8;stutRate=12;fx2Type=4;revAmt=0.35",
      "0:0,0,0.45;1,1,0|15:0,0.4,0.4;1,1,0|39:0,0.2,0.5;1,0.9,0" },

    // ---- DnB (v0.6 追加: 新機能ショーケース) ----
    { "DnB", "Rolling Stutter",
      "osc1Wave=3;osc1Uni=5;osc1Det=28;osc1KeyStart=40;osc1KeyEnd=88;flt1Cutoff=800;"
      "flt1Env=0.8;bars=6;release=300;fx1Type=6;stutAmt=0.85;stutRate=11;fx2Type=4;"
      "revAmt=0.35",
      "0:0,0,0.5;1,1,0|15:0,0.4,0.45;1,1,0|40:0,0.5,0;1,0.05,0" },
    { "DnB", "Vowel Reese",
      "osc1Wave=3;osc1Uni=3;osc1Det=25;osc1KeyStart=26;osc1KeyEnd=58;flt1Type=4;"
      "flt1Cutoff=180;flt1Res=7;flt1Env=0.9;bars=6;fx1Type=1;satAmt=0.5;satDrive=5;"
      "satAlgo=6",
      "0:0,0,0.4;1,1,0|15:R4;0,0.15,0;0.5,0.85,0;1,0.15,0" },
    { "DnB", "Comb Neuro Rise",
      "osc1Wave=4;osc1Uni=3;osc1Det=22;osc1KeyStart=28;osc1KeyEnd=64;flt1Type=5;"
      "flt1Cutoff=100;flt1Res=9;flt1Env=0.88;bars=6;fx1Type=1;satAmt=0.55;satDrive=6;"
      "satAlgo=9",
      "0:0,0,0.4;1,1,0|15:0,0.15,0.4;1,0.9,0|23:0,0.4,0;1,0.9,0" },
    { "DnB", "Amen Gate Build",
      "osc1On=0;noiseType=0;noiseLevel=0.9;noisePitch=1200;noiseRes=1.5;noiseRange=4;"
      "flt1On=0;bars=6;fx1Type=6;stutAmt=0.75;stutRate=14;fx2Type=4;revAmt=0.3",
      "13:R16;0,0.9,0;0.4,0.9,0;0.5,0.15,0;1,0.15,0|12:0,0.4,0.3;1,0.9,0" },
    { "DnB", "Wide Atmos Lift",
      "osc1Wave=1;osc1Uni=5;osc1Det=22;osc1Pan=-0.55;osc1KeyStart=48;osc1KeyEnd=84;"
      "osc2On=1;osc2Wave=1;osc2Coarse=7;osc2Uni=5;osc2Det=18;osc2Pan=0.55;"
      "osc2Level=0.55;osc2KeyStart=48;osc2KeyEnd=84;flt1Cutoff=1200;flt1Env=0.6;"
      "bars=8;release=900;fx1Type=4;revAmt=0.55;revShimmer=0.6;revDecay=0.85",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0|35:0,0.25,0;1,0.75,0|36:0,0.75,0;1,0.25,0|30:0,0.4,0.3;1,0.95,0" },
    { "DnB", "Fast Snare Rush",
      "osc1On=0;noiseType=0;noiseLevel=0.95;noisePitch=1800;noiseRes=1.2;noiseRange=3;"
      "flt1On=0;bars=5;fx1Type=5;duckAmt=0.85;duckRate=14;duckShape=3;fx2Type=4;"
      "revAmt=0.28",
      "13:0,0.3,0.5;1,0.95,0|33:0,0.5,0;1,0.05,0|12:0,0.45,0.3;1,0.9,0" },
    { "DnB", "Liquid Air Rise",
      "osc1Wave=0;osc1Uni=3;osc1Det=12;osc1KeyStart=52;osc1KeyEnd=88;noiseType=1;"
      "noiseLevel=0.3;flt1Cutoff=1500;flt1Env=0.55;bars=7;release=700;humanize=0.35;"
      "fx1Type=2;choAmt=0.4;fx2Type=4;revAmt=0.5;revShimmer=0.55",
      "0:0,0,0.35;1,1,0|13:0,0.35,0.4;1,0.85,0|30:0,0.42,0.3;1,0.9,0" },
    { "DnB", "Jump Up Screech",
      "osc1Wave=4;osc1Uni=5;osc1Det=32;osc1KeyStart=50;osc1KeyEnd=98;flt1Type=2;"
      "flt1Cutoff=1100;flt1Res=7;flt1Env=0.9;bars=6;fx1Type=1;satAmt=0.55;satDrive=6;"
      "satAlgo=7;fx2Type=3;dlyAmt=0.3;dlyTime=8;dlyFb=0.45",
      "0:0,0,0.55;1,1,0|15:0,0.35,0.45;1,1,0" },
    { "DnB", "Techstep Charge",
      "osc1Wave=2;osc1Uni=3;osc1Det=16;osc1KeyStart=32;osc1KeyEnd=72;osc2On=1;"
      "osc2Wave=3;osc2Coarse=-12;osc2Level=0.5;osc2KeyStart=32;osc2KeyEnd=72;"
      "flt1Cutoff=500;flt1Res=4;flt1Env=0.85;bars=6;fx1Type=1;satAmt=0.5;satDrive=5;"
      "satAlgo=4;fx2Type=5;duckAmt=0.5;duckRate=8",
      "0:0,0,0.45;1,1,0|4:0,0,0.45;1,1,0|15:0,0.35,0.4;1,1,0" },

    // ---- Hardstyle (v0.6 追加: 新機能ショーケース) ----
    { "Hardstyle", "Reverse Bass Lift",
      "osc1Wave=3;osc1Uni=5;osc1Det=25;osc1KeyStart=30;osc1KeyEnd=66;flt1Cutoff=450;"
      "flt1Res=4;flt1Env=0.85;bars=6;fx1Type=5;duckAmt=0.9;duckRate=5;duckShape=5;"
      "fx2Type=1;satAmt=0.5;satDrive=5",
      "0:0,0,0.45;1,1,0|15:0,0.35,0.4;1,1,0" },
    { "Hardstyle", "Screech Stutter",
      "osc1Wave=4;osc1Uni=7;osc1Det=45;osc1Spread=1;osc1KeyStart=48;osc1KeyEnd=100;"
      "flt1Type=2;flt1Cutoff=1000;flt1Res=8;flt1Env=0.92;bars=6;fx1Type=6;stutAmt=0.8;"
      "stutRate=11;fx2Type=1;satAmt=0.6;satDrive=7;satAlgo=7",
      "0:0,0,0.55;1,1,0|15:0,0.3,0.45;1,1,0|40:0,0.5,0;1,0.05,0" },
    { "Hardstyle", "Kick Roll Charge",
      "osc1Wave=0;osc1Level=0.95;osc1KeyStart=24;osc1KeyEnd=44;noiseType=2;"
      "noiseLevel=0.4;flt1On=0;bars=6;fx1Type=5;duckAmt=0.9;duckRate=11;duckShape=4;"
      "fx2Type=1;satAmt=0.5;satDrive=6",
      "0:0,0,0.4;1,1,0|13:0,0.3,0.4;1,0.85,0|33:0,0.5,0;1,0.1,0" },
    { "Hardstyle", "Vowel Hardstyle Lead",
      "osc1Wave=3;osc1Uni=7;osc1Det=40;osc1Spread=1;osc1KeyStart=45;osc1KeyEnd=90;"
      "flt1Type=4;flt1Cutoff=300;flt1Res=7;flt1Env=0.9;bars=7;fx1Type=1;satAmt=0.55;"
      "satDrive=6;fx2Type=4;revAmt=0.4",
      "0:0,0,0.5;1,1,0|15:0,0.1,0.3;0.5,0.7,0;1,1,0" },
    { "Hardstyle", "Euphoric Pluck Rise",
      "osc1Wave=2;osc1Uni=5;osc1Det=25;osc1KeyStart=52;osc1KeyEnd=92;attack=1;"
      "release=350;flt1Cutoff=900;flt1Env=0.75;bars=7;fx1Type=3;dlyAmt=0.45;dlyTime=5;"
      "dlyFb=0.55;fx2Type=4;revAmt=0.45",
      "0:0,0,0.4;1,1,0|1:R8;0,0.9,0;0.35,0.9,0;0.4,0.15,0;1,0.15,0|15:0,0.45,0.4;1,1,0" },
    { "Hardstyle", "Raw Comb Riser",
      "osc1Wave=3;osc1Uni=3;osc1Det=20;osc1KeyStart=34;osc1KeyEnd=74;flt1Type=5;"
      "flt1Cutoff=130;flt1Res=10;flt1Env=0.9;bars=6;fx1Type=1;satAmt=0.65;satDrive=8;"
      "satAlgo=1",
      "0:0,0,0.45;1,1,0|15:0,0.15,0.4;1,0.9,0|23:0,0.45,0;1,0.9,0" },
    { "Hardstyle", "Gated Hard Lift",
      "osc1Wave=3;osc1Uni=7;osc1Det=35;osc1Spread=1;osc1KeyStart=40;osc1KeyEnd=88;"
      "flt1Cutoff=700;flt1Env=0.8;bars=7;fx1Type=1;satAmt=0.5;satDrive=5;fx2Type=4;"
      "revAmt=0.35",
      "0:0,0,0.5;1,1,0|1:R16;0,0.9,0;0.45,0.9,0;0.5,0.05,0;1,0.05,0|15:0,0.4,0.4;1,1,0" },
    { "Hardstyle", "Wide Screech Wall",
      "osc1Wave=4;osc1Uni=5;osc1Det=40;osc1Pan=-0.6;osc1KeyStart=45;osc1KeyEnd=93;"
      "osc2On=1;osc2Wave=4;osc2Uni=5;osc2Det=40;osc2Pan=0.6;osc2Fine=10;"
      "osc2Level=0.75;osc2KeyStart=45;osc2KeyEnd=93;flt1Type=2;flt1Cutoff=1200;"
      "flt1Res=6;flt1Env=0.9;bars=6;fx1Type=1;satAmt=0.6;satDrive=7;satAlgo=7",
      "0:0,0,0.55;1,1,0|4:0,0,0.55;1,1,0|15:0,0.3,0.45;1,1,0|35:0,0.2,0;1,0.05,0|36:0,0.8,0;1,0.95,0" },
    { "Hardstyle", "Sub Charge Hard",
      "osc1Wave=0;osc1Level=1;osc1KeyStart=24;osc1KeyEnd=45;osc2On=1;osc2Wave=3;"
      "osc2Coarse=12;osc2Level=0.35;osc2KeyStart=24;osc2KeyEnd=45;flt1Cutoff=250;"
      "flt1Env=0.8;bars=5;fx1Type=1;satAmt=0.55;satDrive=6",
      "0:0,0,0.4;1,1,0|4:0,0,0.4;1,1,0|15:0,0.35,0.4;1,1,0" },
    { "Hardstyle", "Rawstyle Growl Up",
      "osc1Wave=4;osc1Uni=3;osc1Det=25;osc1KeyStart=30;osc1KeyEnd=70;flt1Type=4;"
      "flt1Cutoff=220;flt1Res=9;flt1Env=0.95;bars=6;fx1Type=1;satAmt=0.7;satDrive=9;"
      "satAlgo=1;fx2Type=5;duckAmt=0.6;duckRate=8",
      "0:0,0,0.4;1,1,0|15:R5;0,0.1,0;0.5,0.9,0;1,0.1,0|23:0,0.45,0;1,0.95,0" },

    // ---- Ambient (v0.6 追加: 新機能ショーケース) ----
    { "Ambient", "Breathing Pad Rise",
      "osc1Wave=1;osc1Uni=5;osc1Det=18;osc1Spread=1;osc1KeyStart=45;osc1KeyEnd=76;"
      "flt1Cutoff=900;flt1Env=0.5;bars=9;attack=300;release=2500;humanize=0.7;"
      "fx1Type=2;choAmt=0.4;fx2Type=4;revAmt=0.65;revDecay=0.92;revMod=0.5",
      "0:0,0,0.3;1,1,0|15:0,0.4,0.3;1,0.9,0|30:0,0.45,0.3;1,0.95,0" },
    { "Ambient", "Vowel Cloud",
      "osc1Wave=1;osc1Uni=5;osc1Det=20;osc1KeyStart=48;osc1KeyEnd=78;flt1Type=4;"
      "flt1Cutoff=250;flt1Res=4;flt1Env=0.7;bars=9;attack=400;release=2200;"
      "humanize=0.5;fx1Type=4;revAmt=0.6;revShimmer=0.6;revDecay=0.9",
      "0:0,0,0.3;1,1,0|15:0,0.15,0;0.5,0.85,0;1,0.3,0" },
    { "Ambient", "Comb Chimes",
      "osc1On=0;noiseType=1;noiseLevel=0.7;noisePitch=800;noiseRange=4;flt1Type=5;"
      "flt1Cutoff=300;flt1Res=11;flt1Env=0.8;bars=9;release=2000;fx1Type=4;"
      "revAmt=0.65;revShimmer=0.75;revDecay=0.92",
      "12:0,0.4,0.3;1,0.9,0|13:0,0.25,0.4;1,0.8,0|15:0,0.25,0.3;1,0.9,0" },
    { "Ambient", "Slow Pan Drift",
      "osc1Wave=0;osc1Uni=3;osc1Det=8;osc1Pan=-0.7;osc1KeyStart=48;osc1KeyEnd=72;"
      "osc2On=1;osc2Wave=1;osc2Coarse=7;osc2Uni=3;osc2Det=8;osc2Pan=0.7;osc2Level=0.5;"
      "osc2KeyStart=48;osc2KeyEnd=72;flt1Cutoff=1200;flt1Env=0.45;bars=9;attack=500;"
      "release=3000;humanize=0.6;fx1Type=4;revAmt=0.6;revDecay=0.9;revMod=0.6",
      "0:0,0,0.3;1,1,0|4:0,0,0.3;1,1,0|35:0,0.15,0;0.5,0.85,0;1,0.15,0|36:0,0.85,0;0.5,0.15,0;1,0.85,0" },
    { "Ambient", "Shimmer Bloom Long",
      "osc1Wave=1;osc1Uni=5;osc1Det=15;osc1KeyStart=52;osc1KeyEnd=80;flt1Cutoff=1500;"
      "flt1Env=0.4;bars=9;attack=600;release=3500;fx1Type=4;revAmt=0.7;"
      "revShimmer=0.85;revDecay=0.95;revMod=0.5",
      "0:0,0,0.25;1,1,0|30:0,0.5,0.3;1,1,0|31:0,0.4,0.3;1,0.95,0" },
    { "Ambient", "Whispered Air",
      "osc1On=0;noiseType=1;noiseLevel=0.55;noisePitch=2500;noiseRes=1.5;noiseRange=3;"
      "flt1On=0;bars=9;release=2500;humanize=0.8;fx1Type=4;revAmt=0.6;revDecay=0.9",
      "12:0,0.4,0.3;1,0.8,0|13:0,0.2,0.3;0.6,0.7,0;1,0.35,0" },
    { "Ambient", "Glass Resonance",
      "osc1Wave=0;osc1Uni=3;osc1Det=6;osc1KeyStart=60;osc1KeyEnd=88;scaleOn=1;"
      "scaleKey=0;scaleType=3;flt1Type=5;flt1Cutoff=500;flt1Res=10;flt1Env=0.7;bars=9;"
      "attack=200;release=2500;fx1Type=4;revAmt=0.65;revShimmer=0.7;revDecay=0.92",
      "0:0,0,0;0.25,0.3,0;0.5,0.55,0;0.75,0.8,0;1,1,0|15:0,0.3,0.3;1,0.9,0" },
    { "Ambient", "Tidal Swell",
      "osc1Wave=1;osc1Uni=5;osc1Det=22;osc1Spread=1;osc1KeyStart=40;osc1KeyEnd=70;"
      "flt1Cutoff=700;flt1Env=0.55;bars=9;attack=400;release=3000;humanize=0.5;"
      "fx1Type=4;revAmt=0.6;revDecay=0.9",
      "0:0,0,0.3;1,1,0|1:R2;0,0.3,0;0.5,0.75,0;1,0.3,0|15:0,0.35,0.3;1,0.9,0" },

    // ---- Scale Riser (v0.6 追加: 新機能ショーケース) ----
    { "Scale Riser", "Comb Scale Ladder",
      "osc1On=0;noiseType=1;noiseLevel=0.9;noisePitch=300;noiseRange=4;scaleOn=1;"
      "scaleKey=9;scaleType=2;flt1Type=5;flt1Cutoff=110;flt1Res=10;flt1Env=0.9;bars=7;"
      "fx1Type=4;revAmt=0.35",
      "12:0,0.35,0;0.2,0.5,0;0.4,0.62,0;0.6,0.74,0;0.8,0.86,0;1,1,0|15:0,0.15,0;1,0.9,0" },
    { "Scale Riser", "Vowel Pentatonic Rise",
      "osc1Wave=3;osc1Uni=3;osc1Det=15;osc1KeyStart=36;osc1KeyEnd=84;scaleOn=1;"
      "scaleKey=0;scaleType=4;flt1Type=4;flt1Cutoff=200;flt1Res=6;flt1Env=0.85;bars=7;"
      "fx1Type=4;revAmt=0.4;revShimmer=0.4",
      "0:0,0,0;0.2,0.2,0;0.4,0.4,0;0.6,0.6,0;0.8,0.8,0;1,1,0|15:0,0.1,0;0.5,0.75,0;1,0.35,0" },
    };

    return list;
}

int count() { return (int)items().size(); }

juce::String nameOf(int index)
{
    const auto& its = items();
    if (index < 0 || index >= (int)its.size()) return {};
    return its[(size_t)index].name;
}

// ==========================================================
static void applyParams(LiftXAudioProcessor& proc, const juce::String& s)
{
    for (const auto& tok : juce::StringArray::fromTokens(s, ";", ""))
    {
        const int eq = tok.indexOfChar('=');
        if (eq <= 0) continue;
        const auto id = tok.substring(0, eq).trim();
        const float v = tok.substring(eq + 1).getFloatValue();

        if (auto* rp = proc.apvts.getParameter(id))
        {
            const auto& r = rp->getNormalisableRange();
            rp->setValueNotifyingHost(r.convertTo0to1(juce::jlimit(r.start, r.end, v)));
        }
        else
        {
            jassertfalse; // プリセット内のID誤り
        }
    }
}

// ----------------------------------------------------------------------------
//  再発防止: 本ファイルのカーブ index は下記 enum 値を前提に書かれている。
//  CurveStore::Index を途中挿入で変更すると全プリセットが静かに壊れるため、
//  ここでコンパイル時に固定する。ビルドが通らなくなったら、enum を末尾追加に
//  直すか、プリセットデータ側の index を新しい値へ一括変換すること。
// ----------------------------------------------------------------------------
static_assert(CurveStore::Osc1Pitch  ==  0, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::NoisePitch == 12, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::Filter1    == 15, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::Filter1Res == 19, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::SatAmt     == 23, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::ChoAmt     == 25, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::DlyAmt     == 27, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::RevAmt     == 30, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::DuckAmt    == 32, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::Osc1Pan    == 35, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::StutAmt    == 39, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::Osc1Pos    == 41, "FactoryPresets: curve index layout changed");
static_assert(CurveStore::kNumCurves == 44, "FactoryPresets: curve count changed");

static void applyCurves(LiftXAudioProcessor& proc, const juce::String& s)
{
    for (const auto& seg : juce::StringArray::fromTokens(s, "|", ""))
    {
        const int colon = seg.indexOfChar(':');
        if (colon <= 0) continue;
        const int idx = seg.substring(0, colon).getIntValue();
        if (idx < 0 || idx >= CurveStore::kNumCurves) continue;
        proc.getCurves().publish(idx, CurveSnapshot::fromString(seg.substring(colon + 1)));
    }
}

void apply(LiftXAudioProcessor& proc, int index)
{
    const auto& its = items();
    if (index < 0 || index >= (int)its.size()) return;

    proc.initPreset();
    applyParams(proc, its[(size_t)index].params);
    applyCurves(proc, its[(size_t)index].curves);
}

} // namespace FactoryPresets
