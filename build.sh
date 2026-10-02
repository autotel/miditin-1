#!/usr/bin/env bash
set -e

# ./build.sh            configure + build (VST3, Standalone, tests)
# ./build.sh test       + run the headless suite
# ./build.sh sanitize   build and run the suite under ASan/UBSan instead (slow; test binary only)
if [ "$1" = "sanitize" ]; then
  cmake --preset sanitize
  cmake --build build-asan --target MiditinTests -j"$(nproc)"
  # The suppression file covers fontconfig/freetype's process-lifetime globals, which JUCE pulls
  # in the moment anything asks for a font. See .lsan-suppressions.txt before adding to it.
  ASAN_OPTIONS=detect_leaks=1 \
  LSAN_OPTIONS="suppressions='$(pwd)/.lsan-suppressions.txt':print_suppressions=0" \
  UBSAN_OPTIONS=print_stacktrace=1 \
    ./build-asan/MiditinTests_artefacts/Debug/MiditinTests
  exit
fi

cmake --preset default
cmake --build build -j"$(nproc)"
if [ "$1" = "test" ]; then
  ./build/MiditinTests_artefacts/RelWithDebInfo/MiditinTests
fi
