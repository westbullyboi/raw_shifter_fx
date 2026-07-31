# Framework-independent unit tests for the DSP core. Deliberately does NOT
# depend on JUCE (see Source/DSP + Source/Utility, which have no JUCE
# includes), so these build and run even when SHIFTERFX_BUILD_PLUGIN is OFF
# or JUCE isn't available.

add_executable(ShifterFX_Tests
    main.cpp
    RingBufferTests.cpp
    StepClockTests.cpp
    EffectsTests.cpp
    ShifterEngineTests.cpp
    AllocationTests.cpp
    ${SHIFTERFX_DSP_CORE_SOURCES}
)

target_compile_features(ShifterFX_Tests PRIVATE cxx_std_20)
target_include_directories(ShifterFX_Tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME ShifterFX_UnitTests COMMAND ShifterFX_Tests)
