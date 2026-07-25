// ==========================================
// File: FactoryPresets.cpp
// 即戦力の Riser / Downer プリセット 30種 (コード埋め込み)
//
//  記法:
//   params: "id=value;..." (Choice/Boolはインデックス/0-1)
//   curves: "idx:x,y,c;x,y,c|idx:..."
//  主要インデックス:
//   Wave: 0=Sine 1=Tri 2=Sqr 3=Saw 4=FM / noiseType: 0=White 1=Pink 2=Brown
//   fltType: 0=LP 1=HP 2=BP 3=Notch / fxNType: 0=None 1=Sat 2=Cho 3=Dly 4=Rev 5=Duck
//   bars: 0=1/32..5=1 6=2 7=4 8=8 9=16
//   dlyTime: 5=1/8 8=1/16 / duckRate: 5=1/4 8=1/8 11=1/16
//   カーブidx: 0=O1Pitch 12=NzPitch 13=NzLevel 15-18=Flt1-4
//              19=SatAmt 20=SatDrive 23=DlyAmt 26=RevAmt 28=DuckAmt 29=DuckRate
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
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|26:0,0.4,0.3;1,0.9,0" },

    { "EDM", "White Sweep Up",
      "osc1On=0;noiseType=0;noiseLevel=0.95;noisePitch=250;noiseRes=3.5;noiseRange=6;"
      "flt1On=0;bars=7;release=500;fx1Type=4;revAmt=0.45;revDecay=0.8",
      "12:0,0.35,0.4;1,1,0|13:0,0.25,0.4;1,0.95,0|26:0,0.4,0;1,0.9,0" },

    { "EDM", "Pluck Echo Build",
      "osc1Wave=2;osc1Uni=3;osc1Det=15;osc1KeyStart=60;osc1KeyEnd=84;attack=0.5;release=400;"
      "bars=8;fx1Type=3;dlyAmt=0.5;dlyTime=5;dlyFb=0.65;fx2Type=5;duckAmt=0.5;duckRate=8",
      "0:0,0,0.3;1,1,0|15:0,0.5,0.3;1,1,0|23:0,0.35,0.4;1,0.95,0" },

    { "EDM", "Big Room Lift",
      "osc1Wave=3;osc1Uni=7;osc1Det=45;osc1Spread=1;osc1KeyStart=33;osc1KeyEnd=93;"
      "osc2On=1;osc2Wave=3;osc2Coarse=12;osc2Uni=5;osc2Det=25;osc2Level=0.5;"
      "osc2KeyStart=33;osc2KeyEnd=93;flt1Cutoff=400;flt1Env=0.9;flt1Res=2.5;bars=8;"
      "fx1Type=1;satAmt=0.4;satDrive=4;fx2Type=4;revAmt=0.4",
      "0:0,0,0.55;1,1,0|4:0,0,0.55;1,1,0|15:0,0.4,0.5;1,1,0|26:0,0.42,0.2;1,0.85,0" },

    { "EDM", "Noise Roll Accelerator",
      "osc1On=0;noiseType=0;noiseLevel=0.9;noisePitch=1500;noiseRes=1.2;noiseRange=3;flt1On=0;"
      "bars=6;fx1Type=5;duckAmt=0.85;duckRate=8;duckShape=3;fx2Type=4;revAmt=0.3",
      "13:0,0.3,0.4;1,0.95,0|29:0,0.5,0;1,0.1,0|12:0,0.4,0.3;1,0.9,0" },

    // ================= Trance =================
    { "Trance", "Supersaw Heaven",
      "osc1Wave=3;osc1Uni=7;osc1Det=55;osc1Spread=1;osc1Level=0.75;osc1KeyStart=45;osc1KeyEnd=81;"
      "osc2On=1;osc2Wave=3;osc2Uni=7;osc2Det=35;osc2Coarse=12;osc2Level=0.45;"
      "osc2KeyStart=45;osc2KeyEnd=81;flt1Cutoff=700;flt1Env=0.85;flt1Res=1.2;bars=8;release=600;"
      "fx1Type=2;choAmt=0.5;choDepth=0.6;fx2Type=4;revAmt=0.5;revShimmer=0.55",
      "0:0,0,0.6;1,1,0|4:0,0,0.6;1,1,0|15:0,0.45,0.5;1,1,0|26:0,0.45,0.3;1,0.9,0" },

    { "Trance", "Acid Climb",
      "osc1Wave=3;osc1KeyStart=36;osc1KeyEnd=72;flt1Cutoff=300;flt1Res=8;flt1Env=0.95;bars=7;"
      "fx1Type=1;satAmt=0.55;satDrive=5;satAlgo=2;fx2Type=3;dlyAmt=0.35;dlyTime=8;dlyFb=0.5",
      "0:0,0,0.4;1,1,0|15:0,0.4,0;0.25,0.7,-0.5;0.5,0.5,0;0.75,0.85,-0.5;1,1,0" },

    { "Trance", "Gated Riser",
      "osc1Wave=3;osc1Uni=5;osc1Det=30;osc1KeyStart=45;osc1KeyEnd=93;bars=8;"
      "fx1Type=5;duckAmt=0.7;duckRate=11;duckShape=4;fx2Type=4;revAmt=0.4",
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|28:0,0.6,0;1,0.35,0" },

    { "Trance", "Uplifter 16Bars",
      "osc1Wave=3;osc1Uni=7;osc1Det=25;osc1KeyStart=45;osc1KeyEnd=93;"
      "osc3On=1;osc3Wave=0;osc3Coarse=-12;osc3Level=0.5;osc3KeyStart=45;osc3KeyEnd=93;"
      "noiseType=1;noiseLevel=0.35;bars=9;release=800;"
      "fx1Type=4;revAmt=0.5;revShimmer=0.7;revDecay=0.85",
      "0:0,0,0.7;1,1,0|8:0,0,0.7;1,1,0|13:0,0.3,0.6;1,0.9,0|15:0,0.42,0.6;1,1,0|26:0,0.42,0.4;1,0.95,0" },

    { "Trance", "Dream Shimmer",
      "osc1Wave=1;osc1Uni=3;osc1Det=18;osc1KeyStart=48;osc1KeyEnd=84;flt1Cutoff=1200;flt1Env=0.6;"
      "bars=8;release=900;fx1Type=4;revAmt=0.6;revShimmer=0.8;revDecay=0.85;fx2Type=2;choAmt=0.4",
      "0:0,0,0.45;1,1,0|15:0,0.5,0.3;1,1,0|27:0,0.4,0.3;1,0.95,0" },

    // ================= Bass =================
    { "Bass", "Growl Ramp",
      "osc1Wave=4;osc1KeyStart=24;osc1KeyEnd=60;osc2On=1;osc2Wave=2;osc2Level=0.5;"
      "osc2KeyStart=24;osc2KeyEnd=60;flt1Cutoff=250;flt1Res=5;flt1Env=0.9;bars=6;"
      "fx1Type=1;satAmt=0.6;satDrive=6;satAlgo=7",
      "0:0,0,0;0.5,0.4,0;0.75,0.7,0;1,1,0|4:0,0,0;0.5,0.4,0;0.75,0.7,0;1,1,0|15:0,0.4,0.3;1,1,0|20:0,0.4,0;1,0.85,0" },

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
      "12:0,0.4,0.4;1,0.95,0|13:0,0.3,0.4;1,0.95,0|29:0,0.5,0;0.7,0.3,0;1,0.05,0" },

    { "Bass", "Metal FM Screech",
      "osc1Wave=4;osc1Uni=5;osc1Det=40;osc1KeyStart=48;osc1KeyEnd=96;"
      "flt1Type=1;flt1Cutoff=200;flt1Env=0.5;bars=7;"
      "fx1Type=1;satAmt=0.5;satDrive=7;satAlgo=8;fx2Type=4;revAmt=0.35",
      "0:0,0,0.55;1,1,0|15:0,0.5,0.3;1,1,0|19:0,0.4,0.3;1,0.85,0" },

    // ================= Techno =================
    { "Techno", "Warehouse Lift",
      "osc1Wave=3;osc1Uni=5;osc1Det=35;osc1KeyStart=33;osc1KeyEnd=69;"
      "noiseType=1;noiseLevel=0.4;flt1Cutoff=500;flt1Env=0.85;flt1Res=3;bars=8;"
      "fx1Type=3;dlyAmt=0.4;dlyTime=5;dlyFb=0.55;dlyDamp=0.5;fx2Type=4;revAmt=0.35",
      "0:0,0,0.5;1,1,0|13:0,0.3,0.4;1,0.85,0|15:0,0.42,0.5;1,1,0|23:0,0.4,0.3;1,0.9,0" },

    { "Techno", "Rumble Riser",
      "osc1Wave=0;osc1Level=0.9;osc1KeyStart=24;osc1KeyEnd=36;noiseType=2;noiseLevel=0.7;"
      "noisePitch=120;noiseRes=1.5;noiseRange=2.5;flt1Cutoff=300;flt1Env=0.6;bars=7;"
      "fx1Type=1;satAmt=0.5;satDrive=5;satAlgo=3",
      "0:0,0,0.4;1,1,0|12:0,0.4,0.3;1,0.9,0|13:0,0.4,0.3;1,0.9,0|15:0,0.45,0.3;1,1,0" },

    { "Techno", "Percussive Climb",
      "osc1Wave=2;osc1KeyStart=60;osc1KeyEnd=84;attack=0.3;release=120;bars=6;"
      "fx1Type=5;duckAmt=0.8;duckRate=11;duckShape=6;fx2Type=3;dlyAmt=0.45;dlyTime=8;dlyFb=0.6",
      "0:0,0,0.3;1,1,0|15:0,0.5,0.3;1,1,0|28:0,0.55,0;1,0.3,0" },

    { "Techno", "Hypnotic Notch",
      "osc1Wave=1;osc1Uni=3;osc1Det=22;osc1KeyStart=45;osc1KeyEnd=69;"
      "flt1Type=3;flt1Cutoff=800;flt1Res=4;flt1Env=0.9;"
      "flt2On=1;flt2Cutoff=2000;flt2Env=0.5;bars=8;fx1Type=2;choAmt=0.45;choDepth=0.5",
      "0:0,0,0.4;1,1,0|15:0,0.3,0;0.5,0.9,-0.4;1,0.4,0|16:0,0.45,0.4;1,1,0" },

    { "Techno", "Industrial Noise Wall",
      "osc1On=0;noiseType=0;noiseLevel=1;noisePitch=600;noiseRes=0.8;noiseRange=7;"
      "flt1Type=1;flt1Cutoff=100;flt1Env=0.8;bars=8;"
      "fx1Type=1;satAmt=0.55;satDrive=6;fx2Type=4;revAmt=0.45;revDamp=0.6",
      "12:0,0.3,0.5;1,1,0|13:0,0.35,0.5;1,1,0|15:0,0.4,0.5;1,1,0|19:0,0.4,0.2;1,0.8,0" },

    // ================= Cinematic =================
    { "Cinematic", "Trailer Swell",
      "osc1Wave=3;osc1Uni=7;osc1Det=20;osc1KeyStart=29;osc1KeyEnd=65;"
      "osc3On=1;osc3Wave=0;osc3Coarse=-12;osc3Level=0.6;osc3KeyStart=29;osc3KeyEnd=65;"
      "noiseType=1;noiseLevel=0.45;attack=200;release=1200;bars=8;"
      "flt1Cutoff=350;flt1Env=0.9;fx1Type=4;revAmt=0.65;revDecay=0.9;revShimmer=0.5",
      "0:0,0,0.6;1,1,0|8:0,0,0.6;1,1,0|13:0,0.3,0.5;1,0.95,0|15:0,0.4,0.6;1,1,0|26:0,0.45,0.4;1,0.95,0" },

    { "Cinematic", "Braam Riser",
      "osc1Wave=3;osc1Uni=7;osc1Det=60;osc1KeyStart=24;osc1KeyEnd=48;bars=7;release=1000;"
      "flt1Cutoff=450;flt1Env=0.7;fx1Type=1;satAmt=0.6;satDrive=8;satAlgo=1;fx2Type=4;revAmt=0.5;revDecay=0.8",
      "0:0,0,0.35;1,1,0|15:0,0.42,0.4;1,1,0|20:0,0.45,0.2;1,0.8,0" },

    { "Cinematic", "Ethereal Rise",
      "osc1Wave=0;osc1Uni=5;osc1Det=15;osc1KeyStart=60;osc1KeyEnd=96;"
      "noiseType=1;noiseLevel=0.2;noiseRange=6;bars=9;release=1500;flt1On=0;"
      "fx1Type=4;revAmt=0.7;revShimmer=0.85;revDecay=0.9;fx2Type=2;choAmt=0.5",
      "0:0,0,0.7;1,1,0|12:0,0.4,0.5;1,1,0|13:0,0.3,0.5;1,0.8,0|26:0,0.5,0.3;1,0.95,0" },

    { "Cinematic", "Tension Strings",
      "osc1Wave=3;osc1Uni=7;osc1Det=18;osc1Spread=0.9;osc1KeyStart=52;osc1KeyEnd=76;"
      "flt1Type=2;flt1Cutoff=900;flt1Res=5;flt1Env=0.6;bars=8;"
      "fx1Type=5;duckAmt=0.6;duckRate=8;duckShape=2;fx2Type=4;revAmt=0.5",
      "0:0,0,0.5;1,1,0|15:0,0.45,0.4;1,1,0|28:0,0.5,0;1,0.75,0" },

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
      "0:0,0,-0.6;1,1,0|23:0,0.5,0;1,0.8,0" },

    { "Downer", "Tape Stop",
      "osc1Wave=3;osc1Uni=5;osc1Det=25;osc1KeyStart=60;osc1KeyEnd=24;bars=4;release=300;"
      "flt1Cutoff=3000;flt1Env=-0.8;fx1Type=1;satAmt=0.5;satDrive=5;satAlgo=3",
      "0:0,0,0.7;1,1,0|15:0,0.5,0.5;1,1,0" },

    { "Downer", "Reverse Shimmer Down",
      "osc1Wave=1;osc1Uni=3;osc1Det=20;osc1KeyStart=84;osc1KeyEnd=48;"
      "noiseType=1;noiseLevel=0.3;bars=7;release=1200;flt1Cutoff=2500;flt1Env=-0.5;"
      "fx1Type=4;revAmt=0.65;revShimmer=0.8;revDecay=0.88;fx2Type=2;choAmt=0.4",
      "0:0,0,0.45;1,1,0|13:0,0.7,0;1,0.25,0|15:0,0.5,0.3;1,1,0|26:0,0.45,0.3;1,0.9,0" },
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
