cmake_minimum_required(VERSION 3.22)
project(RMX16 VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(RMX_JUCE_PATH "" CACHE PATH "Local JUCE 8.0.6 checkout")
if(RMX_JUCE_PATH)
    add_subdirectory("${RMX_JUCE_PATH}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
                         GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()
juce_add_plugin(RMX16
    COMPANY_NAME "Rack Digital"
    BUNDLE_ID org.rmx16.effect
    PLUGIN_MANUFACTURER_CODE JkAU
    PLUGIN_CODE Rx16
    PRODUCT_NAME "RMX16"
    FORMATS VST3
    VST3_CATEGORIES Fx Reverb
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(RMX16 PRIVATE src/Processor.cpp src/Panel.cpp)
target_compile_definitions(RMX16 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(RMX16 PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
add_executable(rmx_bench tests/bench.cpp)
