#!/usr/bin/env bash
# Formal macOS export: configures a fresh Release build (separate from your
# everyday dev "build" dir so this never picks up stray debug settings),
# confirms it's a universal (arm64+x86_64) binary, and zips VST3 + AU +
# Standalone plus install instructions into dist/ ready to hand to testers.
#
# AAX is intentionally left OUT of the tester zip (Pro Tools support is a
# later phase) even though it still builds as part of AnalogDrive_All —
# no harm in it compiling, it's just not something testers need yet.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

VERSION="$(grep -m1 -oE 'VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | awk '{print $2}')"
BUILD_DIR="build-release"
DIST_NAME="AnalogDrive-macOS-v${VERSION}"
DIST_DIR="dist/${DIST_NAME}"

echo "== Configuring Release build (universal arm64+x86_64) =="
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release

echo "== Building (VST3 + AU + AAX + Standalone) =="
cmake --build "$BUILD_DIR" --target AnalogDrive_All -j"$(sysctl -n hw.ncpu)"

echo "== Verifying universal binary =="
for f in \
  "$BUILD_DIR/AnalogDrive_artefacts/Release/VST3/Analog Drive.vst3/Contents/MacOS/Analog Drive" \
  "$BUILD_DIR/AnalogDrive_artefacts/Release/AU/Analog Drive.component/Contents/MacOS/Analog Drive"
do
  archs="$(lipo -info "$f" | sed 's/.*: //')"
  echo "  $f -> $archs"
  case "$archs" in
    *x86_64*arm64*|*arm64*x86_64*) ;;
    *) echo "ERROR: $f is not a universal binary (got: $archs)"; exit 1 ;;
  esac
done

echo "== Packaging =="
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"
# Testers get the installer only, not loose bundles: hand-copied bundles stay
# quarantined and won't load in Ableton/Logic (see build_installer_macos.sh).
scripts/build_installer_macos.sh "$BUILD_DIR/AnalogDrive_artefacts/Release" \
  "$DIST_DIR/AnaDrive_Installer_1.pkg" "$VERSION"
cp "scripts/TESTER_README_macOS.txt" "$DIST_DIR/READ ME FIRST.txt"

( cd dist && rm -f "${DIST_NAME}.zip" && zip -rq "${DIST_NAME}.zip" "${DIST_NAME}" )

echo ""
echo "== Done: dist/${DIST_NAME}.zip =="
