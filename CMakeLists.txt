
cmake_minimum_required(VERSION 3.25.0)

project(smtg-vst3-vinylcontroller
    VERSION 2.2.0.0
    DESCRIPTION "Steinberg VST 3 Bassbuster Vinyl Controller"
)

if(NOT SMTG_ENABLE_VSTGUI_SUPPORT)
    return()
endif()

set(target vinylcontroller)

smtg_add_vst3plugin(${target}
    source/entry.cpp
    source/version.h
    source/vinylcids.h
    source/vinylcontroller.cpp
    source/vinyleditor.cpp
    source/vinylparamids.h
    source/vinylprocessor.h
    source/vinylconfigconst.h
    source/vinylcontroller.h
    source/vinyleditor.h
    source/vinylprocessor.cpp
    source/controls/cvinylbuttons.cpp
    source/controls/cvinylbuttons.h
    source/controls/cvinylpopupmenu.cpp
    source/controls/cvinylpopupmenu.h
    source/controls/cwaveview.cpp
    source/controls/cwaveview.h
    source/controls/cdebugfftview.cpp
    source/controls/cdebugfftview.h
    source/helpers/sampleentry.h
    source/helpers/parameterreader.h
    source/helpers/parameterwriter.h
    source/helpers/cuepoint.h
    source/helpers/padentry.h
    source/helpers/fft.h
    source/helpers/fft.cpp
    source/helpers/resourcepath.h
    source/helpers/resourcepath.cpp
    source/helpers/ringbuffer.h
    source/helpers/filtred.h
)

set_target_properties(${target} 
    PROPERTIES
        ${SDK_IDE_PLUGIN_EXAMPLES_FOLDER}
)

target_compile_features(${target}
    PUBLIC
        cxx_std_17
)

target_link_libraries(${target}
    PRIVATE
        vstgui_support
        vstgui_standalone
)

smtg_target_add_plugin_resources(${target}
    RESOURCES
        #resource/vinyl.uidesc
        resource/background.png
        resource/freeze.png
        resource/gain_back.png
        resource/locktone.png
        resource/pitch_handler.png
        resource/revers_check_on.png
        resource/sync_check.png
        resource/curve_1.png
        resource/fx_blue.png
        resource/glass.png
        resource/loop_check_on.png
        resource/pitch_slider.png
        resource/revers_check.png
        resource/tclearn_check_on.png
        resource/volume_handler.png
        resource/curve_2.png
        resource/fx_empty.png
        resource/hold.png
        resource/loop_check.png
        resource/postroll.png
        resource/sample_combo.png
        resource/tclearn_check.png
        resource/volume_slider.png
        resource/curve_3.png
        resource/fx_green.png
        resource/kick_button_act.png
        resource/pitch_1.png
        resource/preroll.png
        resource/smal_handler.png
        resource/vintage.png
        resource/vu_off.png
        resource/digits.png
        resource/fx_red.png
        resource/kick_button.png
        resource/pitch_2.png
        resource/punchin.png
        resource/smal_slider.png
        resource/vu_on.png
        resource/distorsion.png
        resource/fx_yellow.png
        resource/knob_handle.png
        resource/pitch_3.png
        resource/punchout.png
        resource/sync_check_on.png
        resource/yellow_combo.png

        resource/vintage.wav
)

smtg_target_setup_options(${target}
    BUNDLE_IDENTIFIER "com.steinberg.vst3.${target}"
    COMPANY_NAME "Steinberg Media Technologies"
)
