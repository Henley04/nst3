{
  "targets": [
    {
      "target_name": "nst3",
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-fno-exceptions", "-fno-rtti"],
      "cflags_cc": ["-fexceptions", "-frtti", "-std=c++17", "-fvisibility=hidden"],
      "defines": [
        "NAPI_VERSION=8",
        "NAPI_CPP_EXCEPTIONS",
        "NODE_ADDON_API_DISABLE_DEPRECATED",
        "DEVELOPMENT_ENVIRONMENT=1",
        "RELEASE=1"
      ],
      "include_dirs": [
        "<!(node -p \"require('node-addon-api').include_dir\")",
        "src",
        "third_party/vst3sdk",
        "third_party/vst3sdk/base/source",
        "third_party/vst3sdk/public.sdk/source",
        "third_party/vst3sdk/public.sdk/source/vst",
        "third_party/vst3sdk/public.sdk/source/vst/hosting",
        "third_party/vst3sdk/public.sdk/source/vst/utility",
        "third_party/vst3sdk/public.sdk/source/common",
        "third_party/vst3sdk/pluginterfaces",
        "third_party/vst3sdk/pluginterfaces/base",
        "third_party/vst3sdk/pluginterfaces/vst",
        "third_party/vst3sdk/pluginterfaces/gui",
        "third_party/vst3sdk/pluginterfaces/component"
      ],
      "sources": [
        "src/addon.cc",
        "src/version.cc",
        "src/errors.cc",
        "src/host.cc",
        "src/plugin_instance.cc",
        "src/host_application.cc",
        "src/component_handler.cc",
        "src/buffer_stream.cc",
        "src/discovery.cc",
        "src/midi.cc",
        "src/string_convert.cc",
        # VST3 SDK: base
        "third_party/vst3sdk/base/source/baseiids.cpp",
        "third_party/vst3sdk/base/source/fbuffer.cpp",
        "third_party/vst3sdk/base/source/fdebug.cpp",
        "third_party/vst3sdk/base/source/fdynlib.cpp",
        "third_party/vst3sdk/base/source/fobject.cpp",
        "third_party/vst3sdk/base/source/fstreamer.cpp",
        "third_party/vst3sdk/base/source/fstring.cpp",
        "third_party/vst3sdk/base/source/timer.cpp",
        "third_party/vst3sdk/base/source/updatehandler.cpp",
        # VST3 SDK: base/thread
        "third_party/vst3sdk/base/thread/source/flock.cpp",
        "third_party/vst3sdk/base/thread/source/fcondition.cpp",
        # VST3 SDK: public.sdk common
        "third_party/vst3sdk/public.sdk/source/common/commoniids.cpp",
        "third_party/vst3sdk/public.sdk/source/common/commonstringconvert.cpp",
        "third_party/vst3sdk/public.sdk/source/common/memorystream.cpp",
        "third_party/vst3sdk/public.sdk/source/common/pluginview.cpp",
        # ThreadChecker: all three platform variants are guarded by SMTG_OS_*
        # macros, so only the one matching the build OS is actually compiled.
        "third_party/vst3sdk/public.sdk/source/common/threadchecker_win32.cpp",
        "third_party/vst3sdk/public.sdk/source/common/threadchecker_linux.cpp",
        "third_party/vst3sdk/public.sdk/source/common/threadchecker_mac.mm",
        # VST3 SDK: public.sdk vst hosting
        "third_party/vst3sdk/public.sdk/source/vst/hosting/module.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/hostclasses.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/pluginterfacesupport.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/plugprovider.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/connectionproxy.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/parameterchanges.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/eventlist.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/hosting/processdata.cpp",
        # VST3 SDK: public.sdk vst utility
        "third_party/vst3sdk/public.sdk/source/vst/utility/stringconvert.cpp",
        "third_party/vst3sdk/public.sdk/source/vst/utility/systemtime.cpp",
        # VST3 SDK: iid definitions (defines all Steinberg::Vst::IXXX::iid symbols)
        "third_party/vst3sdk/public.sdk/source/vst/vstinitiids.cpp",
        # VST3 SDK: plugin interfaces
        "third_party/vst3sdk/pluginterfaces/base/conststringtable.cpp",
        "third_party/vst3sdk/pluginterfaces/base/coreiids.cpp",
        "third_party/vst3sdk/pluginterfaces/base/funknown.cpp",
        "third_party/vst3sdk/pluginterfaces/base/ustring.cpp"
      ],
      "conditions": [
        [
          "OS=='mac'",
          {
            "sources+": [
              "third_party/vst3sdk/public.sdk/source/vst/hosting/module_mac.mm"
            ],
            "xcode_settings": {
              "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
              "MACOSX_DEPLOYMENT_TARGET": "10.13",
              "GCC_ENABLE_CPP_EXCEPTIONS": "YES",
              "GCC_ENABLE_CPP_RTTI": "YES",
              "CLANG_ENABLE_OBJC_ARC": "YES",
              "OTHER_LDFLAGS": ["-framework", "Foundation", "-framework", "CoreFoundation"],
              "OTHER_CFLAGS": ["-fvisibility=hidden"]
            }
          }
        ],
        [
          "OS=='linux'",
          {
            "sources+": [
              "third_party/vst3sdk/public.sdk/source/vst/hosting/module_linux.cpp"
            ],
            "defines+": ["_GNU_SOURCE"],
            "libraries": ["-ldl", "-lpthread"]
          }
        ],
        [
          "OS=='win'",
          {
            "sources+": [
              "third_party/vst3sdk/public.sdk/source/vst/hosting/module_win32.cpp"
            ],
            "defines+": [
              "_WINDOWS",
              "_WIN32",
              "WIN32",
              "_CRT_SECURE_NO_WARNINGS",
              "_SCL_SECURE_NO_WARNINGS",
              "NOMINMAX"
            ],
            "msbuild_settings": {
              "ClCompile": {
                "LanguageStandard": "stdcpp17",
                "ExceptionHandling": "Async",
                "RuntimeTypeInfo": "true",
                "PreprocessorDefinitions": [
                  "NAPI_VERSION=8",
                  "NAPI_CPP_EXCEPTIONS",
                  "DEVELOPMENT_ENVIRONMENT=1",
                  "_WINDOWS",
                  "_WIN32",
                  "WIN32",
                  "_CRT_SECURE_NO_WARNINGS",
                  "NOMINMAX"
                ]
              },
              "Link": {
                "AdditionalDependencies": ["kernel32.lib", "user32.lib", "advapi32.lib"]
              }
            }
          }
        ]
      ]
    }
  ]
}
