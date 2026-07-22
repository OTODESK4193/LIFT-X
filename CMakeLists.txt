cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  LIFT-X  -  Riser MIDI Synthesizer (OTODESK)
#  JUCE 8.0 系 / VST3 + Standalone
#  開発計画書準拠: DSP/GUI完全分離 / Ableton Live最適化 / SR 44.1-192kHz対応
#  ※ 計画書の juce_audio_processors_headless は JUCE 8 に存在しないため、
#    SPECTRA8 同様の標準モジュール構成 + ソースレベルの DSP/GUI 分離で
#    同じ目的（テスト容易性・依存断絶）を達成する。
# ============================================================================
project(LIFTX VERSION 0.2.0 LANGUAGES C CXX)

# ----------------------------------------------------------------------------
#  C++20
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
#  JUCE の場所
# ----------------------------------------------------------------------------
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to the JUCE checkout")
add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)

# ----------------------------------------------------------------------------
#  プラグイン定義
# ----------------------------------------------------------------------------
juce_add_plugin(LIFTX
    COMPANY_NAME             "OTODESK"
    PLUGIN_MANUFACTURER_CODE "Otdk"
    PLUGIN_CODE              "Lftx"
    FORMATS                  VST3 Standalone
    PRODUCT_NAME             "LIFT-X"
    PLUGIN_DESCRIPTION       "Riser MIDI Synthesizer with Multi-Envelope Curves"
    IS_SYNTH                 TRUE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_AUTO_MANIFEST       FALSE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    VERSION                  "0.2.0"
)

# ----------------------------------------------------------------------------
#  ソース
# ----------------------------------------------------------------------------
target_sources(LIFTX PRIVATE
    # --- プラグインコア ---
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp

    # --- DSP モジュール (GUIヘッダを一切 include しない) ---
    Source/DSP/Wavetable.h
    Source/DSP/CurveData.h
    Source/DSP/ZdfFilter.h
    Source/DSP/RiserEngine.h
    Source/DSP/FxChain.h

    # --- GUI モジュール ---
    Source/GUI/ColorPalette.h
    Source/GUI/ArcDial.h
    Source/GUI/ArcDial.cpp
    Source/GUI/ValueKnob.h
    Source/GUI/GlowToggle.h
    Source/GUI/CurveEditor.h
    Source/GUI/CurveEditor.cpp
    Source/GUI/WaveDisplay.h
    Source/GUI/WavetableBrowser.h
    Source/GUI/WavetableBrowser.cpp
    Source/GUI/MainPanel.h
    Source/GUI/MainPanel.cpp
    Source/GUI/OscEnvPanel.h
    Source/GUI/OscEnvPanel.cpp
    Source/GUI/FilterPanel.h
    Source/GUI/FilterPanel.cpp
    Source/GUI/FxPanel.h
    Source/GUI/FxPanel.cpp
)

# ----------------------------------------------------------------------------
#  インクルードパス
# ----------------------------------------------------------------------------
target_include_directories(LIFTX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/GUI
)

# ----------------------------------------------------------------------------
#  JUCE モジュール
# ----------------------------------------------------------------------------
target_link_libraries(LIFTX
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
#  コンパイル定義
# ----------------------------------------------------------------------------
target_compile_definitions(LIFTX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    LIFTX_VERSION="0.2.0"
)

# ----------------------------------------------------------------------------
#  JUCE ヘッダー生成
# ----------------------------------------------------------------------------
juce_generate_juce_header(LIFTX)

# ----------------------------------------------------------------------------
#  MSVC コンパイルオプション（SPECTRA8同様のスカラー設計。
#  SoAデータレイアウト + タイトループで自動ベクトル化に任せる）
# ----------------------------------------------------------------------------
if(MSVC)
    target_compile_options(LIFTX PRIVATE
        /fp:fast
        /utf-8
        /W3
        /wd4819
        /external:W0
        $<$<CONFIG:Release>:/O2>
    )
endif()
