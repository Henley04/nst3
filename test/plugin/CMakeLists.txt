cmake_minimum_required(VERSION 3.14)
project(Gain VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point to the bundled VST3 SDK (a git submodule of the nst3 repo).
set(VST3_SDK_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vst3sdk)

# Plugin + factory sources.
set(GAIN_SOURCES
    source/gainprocessor.cpp
    source/factory.cpp
)

# Required SDK sources (mirrors the subset the nst3 host addon compiles,
# minus the hosting-side files we don't need for a plugin).
set(VST3_SDK_SOURCES
    ${VST3_SDK_ROOT}/public.sdk/source/vst/vstsinglecomponenteffect.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/vst/vstbus.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/vst/vstcomponent.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/vst/vstcomponentbase.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/vst/vstparameters.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/vst/vstinitiids.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/main/pluginfactory.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/main/moduleinit.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/common/commoniids.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/common/commonstringconvert.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/common/memorystream.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/common/pluginview.cpp
    ${VST3_SDK_ROOT}/public.sdk/source/vst/utility/stringconvert.cpp
    ${VST3_SDK_ROOT}/base/source/baseiids.cpp
    ${VST3_SDK_ROOT}/base/source/fbuffer.cpp
    ${VST3_SDK_ROOT}/base/source/fdebug.cpp
    ${VST3_SDK_ROOT}/base/source/fobject.cpp
    ${VST3_SDK_ROOT}/base/source/fstreamer.cpp
    ${VST3_SDK_ROOT}/base/source/fstring.cpp
    ${VST3_SDK_ROOT}/base/source/timer.cpp
    ${VST3_SDK_ROOT}/base/source/updatehandler.cpp
    ${VST3_SDK_ROOT}/base/thread/source/flock.cpp
    ${VST3_SDK_ROOT}/base/thread/source/fcondition.cpp
    ${VST3_SDK_ROOT}/pluginterfaces/base/conststringtable.cpp
    ${VST3_SDK_ROOT}/pluginterfaces/base/coreiids.cpp
    ${VST3_SDK_ROOT}/pluginterfaces/base/funknown.cpp
    ${VST3_SDK_ROOT}/pluginterfaces/base/ustring.cpp
)

add_library(Gain MODULE ${GAIN_SOURCES} ${VST3_SDK_SOURCES})

target_include_directories(Gain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${VST3_SDK_ROOT}
    ${VST3_SDK_ROOT}/base/source
    ${VST3_SDK_ROOT}/public.sdk/source
    ${VST3_SDK_ROOT}/public.sdk/source/vst
    ${VST3_SDK_ROOT}/public.sdk/source/common
    ${VST3_SDK_ROOT}/pluginterfaces
    ${VST3_SDK_ROOT}/pluginterfaces/base
    ${VST3_SDK_ROOT}/pluginterfaces/vst
    ${VST3_SDK_ROOT}/pluginterfaces/component
    ${VST3_SDK_ROOT}/pluginterfaces/gui
)

target_compile_definitions(Gain PRIVATE
    DEVELOPMENT_ENVIRONMENT=1
    RELEASE=1
)

if(APPLE)
    target_sources(Gain PRIVATE ${VST3_SDK_ROOT}/public.sdk/source/main/macmain.cpp)
    set_target_properties(Gain PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION vst3
        OUTPUT_NAME "Gain"
    )
    target_link_libraries(Gain PRIVATE
        "-framework Foundation"
        "-framework CoreFoundation"
    )
    # NOTE: -fvisibility=hidden is intentionally omitted — the VST3 SDK's
    # UpdateHandler complete-object constructor must remain externally visible
    # for the host to dlopen the plugin without a symbol lookup error.
    target_compile_options(Gain PRIVATE -frtti -fexceptions)
    target_compile_definitions(Gain PRIVATE MAC=1 _DEBUG=1 DEVELOPMENT=1)
    # Install moduleinfo.json into the bundle's Contents/ directory (VST3
    # 3.6.10+ spec location). MACOSX_PACKAGE_LOCATION=Resources would put
    # it inside Contents/Resources/, but the spec wants it at Contents/
    # itself; we use a post-build copy to place it correctly.
    add_custom_command(TARGET Gain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/moduleinfo.json"
                "$<TARGET_BUNDLE_DIR:Gain>/Contents/moduleinfo.json"
        COMMENT "[Gain] Installing moduleinfo.json into bundle"
    )
elseif(UNIX)
    target_sources(Gain PRIVATE ${VST3_SDK_ROOT}/public.sdk/source/main/linuxmain.cpp)
    # Bundle layout on Linux: Gain.vst3/Contents/x86_64-linux/Gain.so
    # Use a per-config output dir so the .so always lands at a known path.
    set_target_properties(Gain PROPERTIES
        PREFIX ""
        OUTPUT_NAME "Gain"
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Gain.vst3/Contents/x86_64-linux"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/Gain.vst3/Contents/x86_64-linux"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/Gain.vst3/Contents/x86_64-linux"
    )
    target_link_libraries(Gain PRIVATE dl pthread)
    # NOTE: -fvisibility=hidden omitted (see macOS comment above).
    target_compile_options(Gain PRIVATE -frtti -fexceptions)
    target_compile_definitions(Gain PRIVATE LINUX=1 _GNU_SOURCE)
    # Create the bundle structure (Resources dir) as a post-build step.
    add_custom_command(TARGET Gain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${CMAKE_BINARY_DIR}/Gain.vst3/Contents/Resources"
        COMMENT "[Gain] Creating bundle Resources directory"
    )
    # Copy moduleinfo.json into the bundle so VST3 3.6.10+ hosts can read
    # the plugin's class list without loading the .so. Source location is
    # next to the CMakeLists.txt so it travels with the source tree.
    add_custom_command(TARGET Gain POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/moduleinfo.json"
                "${CMAKE_BINARY_DIR}/Gain.vst3/Contents/moduleinfo.json"
        COMMENT "[Gain] Installing moduleinfo.json into bundle"
    )
elseif(WIN32)
    target_sources(Gain PRIVATE ${VST3_SDK_ROOT}/public.sdk/source/main/dllmain.cpp)
    # The host loader (module_win32.cpp) handles two layouts:
    #   - flat DLL:   <dir>/Gain.vst3 (a file)        → LoadLibraryW(path)
    #   - bundle dir: <dir>/Gain.vst3/Contents/x86_64-win/Gain.vst3
    # We use the flat-DLL layout: helpers.js points at `build/Gain.vst3`
    # directly. With the Visual Studio multi-config generator the DLL would
    # normally land at `build/Release/Gain.vst3`; we override both the global
    # and per-config output dirs to force it to `build/Gain.vst3` so the path
    # matches Linux/macOS bundle root and the JS test helpers find it.
    set_target_properties(Gain PROPERTIES
        PREFIX ""
        OUTPUT_NAME "Gain"
        SUFFIX ".vst3"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CFG} CFG_UPPER)
        set_target_properties(Gain PROPERTIES
            LIBRARY_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} "${CMAKE_BINARY_DIR}"
        )
    endforeach()
    target_compile_definitions(Gain PRIVATE
        WINDOWS=1 _WIN32 _WINDOWS NOMINMAX
        _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS
    )
endif()
