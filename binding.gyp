{
  "targets": [
    {
      "target_name": "alphafix",
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "sources": [ "alphafix.cpp", "png.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],
      'defines': [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
      'comment': "below enables max optimization (didn't help)",
      "conditions": [
        ["OS=='linux'", {
          "cflags": [ "-O3", "-flto", "-funroll-loops", "-fomit-frame-pointer" ],
          "cflags_cc": [ "-O3", "-flto", "-funroll-loops", "-fomit-frame-pointer", "-std=c++17", "<!@(pkg-config --cflags zlib libpng)" ],
          "ldflags": [ "-flto", "-O3" ],
          "libraries": [
            "<!@(pkg-config --libs zlib libpng)"
          ]
        }],
        ["OS=='mac'", {
          "xcode_settings": {
            "GCC_OPTIMIZATION_LEVEL": "3",
            "LLVM_LTO": "YES",
            "CLANG_CXX_LANGUAGE_STANDARD": "c++17",
            "OTHER_CFLAGS": [ "-O3", "-funroll-loops", "<!@(pkg-config --cflags zlib libpng)" ],
            "OTHER_LDFLAGS": [ "-flto" ]
          },
          "libraries": [ "<!@(pkg-config --libs zlib libpng)" ]
        }],
        ["OS=='win'", {
          "msvs_settings": {
            "VCCLCompilerTool": {
              "Optimization": 3,
              "FavorSizeOrSpeed": 1,
              "WholeProgramOptimization": "true",
              "EnableIntrinsicFunctions": "true",
              "AdditionalOptions": [ "/GL" ]
            },
            "VCLinkerTool": {
              "LinkTimeCodeGeneration": 1,
              "AdditionalOptions": [ "/LTCG", "/NODEFAULTLIB:msvcrt" ]
            }
          }
        }],
      ],
    }
  ]
}
