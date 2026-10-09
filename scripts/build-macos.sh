#!/usr/bin/env bash
# Builds Reckless DJ FX (VST3 + AU + Standalone) and runs the unit tests.
#
#   scripts/build-macos.sh [--install] [--universal] [--juce /path/to/JUCE]
#
#   --install    copy the plugins to ~/Library/Audio/Plug-Ins/{VST3,Components}
#   --universal  build arm64 + x86_64 (default: this Mac's architecture)
#   --juce DIR   use a local JUCE checkout instead of downloading it
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build}"
INSTALL=0
ARCHS="$(uname -m)"
JUCE_ARG=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install) INSTALL=1 ;;
    --universal) ARCHS="arm64;x86_64" ;;
    --juce) JUCE_ARG=(-DFETCHCONTENT_SOURCE_DIR_JUCE="$2"); shift ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

command -v cmake >/dev/null || { echo "cmake not found (brew install cmake / pip3 install cmake)" >&2; exit 1; }

# Some Command Line Tools installs ship an incomplete /usr/include/c++/v1 that hides the SDK's libc++
# headers. Detect it and point clang at the SDK headers instead.
if ! echo '#include <algorithm>' | clang++ -x c++ -fsyntax-only - 2>/dev/null; then
  SDK="$(xcrun --show-sdk-path)"
  export CXXFLAGS="${CXXFLAGS:-} -nostdinc++ -isystem $SDK/usr/include/c++/v1"
  echo "note: using libc++ headers from $SDK"
fi

cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCHS" "${JUCE_ARG[@]}"
cmake --build "$BUILD" --config Release -j "$(sysctl -n hw.ncpu)"

"$BUILD/RecklessDJFXTests_artefacts/Release/RecklessDJFXTests"

ART="$BUILD/RecklessDJFX_artefacts/Release"
if [[ $INSTALL -eq 1 ]]; then
  mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
  rm -rf ~/Library/Audio/Plug-Ins/VST3/"Reckless DJ FX.vst3" ~/Library/Audio/Plug-Ins/Components/"Reckless DJ FX.component"
  cp -R "$ART/VST3/Reckless DJ FX.vst3" ~/Library/Audio/Plug-Ins/VST3/
  cp -R "$ART/AU/Reckless DJ FX.component" ~/Library/Audio/Plug-Ins/Components/
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  echo "Installed to ~/Library/Audio/Plug-Ins"
fi

echo "Built: $ART"
