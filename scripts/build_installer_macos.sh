#!/usr/bin/env bash
# Builds a double-clickable macOS Installer package (.pkg) from a Release
# artefacts folder. Shared by package_macos.sh and CI.
#
# Why a .pkg instead of "drag these into ~/Library": files written by Apple's
# Installer don't inherit the download quarantine flag, so testers approve the
# installer once and the plugins then load in Ableton/Logic. Manually copied
# bundles stay quarantined, and an unnotarized quarantined plugin is silently
# refused by the DAW with no "Open Anyway" escape hatch.
#
# Usage: build_installer_macos.sh <artefacts/Release dir> <output.pkg> <version>
# Optional: INSTALLER_SIGN_ID="Developer ID Installer: ..." to sign the .pkg
# (once there's a paid Apple Developer account).
set -euo pipefail

ARTEFACTS="$1"
OUT_PKG="$2"
VERSION="$3"
ID_PREFIX="com.saturationsounds.analogdrive"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/pkgs"

# make_component_pkg <short id> <bundle path> <install location>
make_component_pkg() {
  local id="$1" src="$2" dest="$3"
  local root="$WORK/root-$id" plist="$WORK/$id.plist"
  mkdir -p "$root"
  ditto "$src" "$root/$(basename "$src")"
  xattr -cr "$root"

  # VST3, AU and Standalone all share one bundle ID. Left relocatable,
  # Installer would "upgrade" whichever copy it finds first anywhere on disk
  # instead of installing to $dest, so pin every bundle to its location.
  pkgbuild --analyze --root "$root" "$plist" >/dev/null
  local i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$plist" >/dev/null 2>&1; do
    # --analyze omits the key for plugin bundles, so Delete+Add, not Set.
    /usr/libexec/PlistBuddy -c "Delete :$i:BundleIsRelocatable" "$plist" >/dev/null 2>&1 || true
    /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$plist"
    i=$((i + 1))
  done

  pkgbuild --quiet \
    --root "$root" \
    --component-plist "$plist" \
    --identifier "$ID_PREFIX.$id" \
    --version "$VERSION" \
    --install-location "$dest" \
    "$WORK/pkgs/$id.pkg"
}

make_component_pkg vst3 "$ARTEFACTS/VST3/Analog Drive.vst3"           "/Library/Audio/Plug-Ins/VST3"
make_component_pkg au   "$ARTEFACTS/AU/Analog Drive.component"        "/Library/Audio/Plug-Ins/Components"
make_component_pkg app  "$ARTEFACTS/Standalone/Analog Drive.app"      "/Applications"

# Payload-free package that always runs: clears out stale manual installs and
# refreshes the AU cache (see installer/scripts/postinstall).
chmod +x "$SCRIPT_DIR/installer/scripts/postinstall"
pkgbuild --quiet --nopayload \
  --scripts "$SCRIPT_DIR/installer/scripts" \
  --identifier "$ID_PREFIX.postinstall" \
  --version "$VERSION" \
  "$WORK/pkgs/postinstall.pkg"

# hostArchitectures lists both so Apple Silicon Macs don't get a pointless
# "install Rosetta?" prompt; the plugins themselves are universal binaries.
cat > "$WORK/Distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Analog Drive $VERSION Beta</title>
    <welcome file="welcome.html" mime-type="text/html"/>
    <options customize="allow" require-scripts="false" rootVolumeOnly="true" hostArchitectures="x86_64,arm64"/>
    <domains enable_localSystem="true"/>
    <allowed-os-versions>
        <os-version min="10.13"/>
    </allowed-os-versions>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
        <line choice="postinstall"/>
    </choices-outline>
    <choice id="vst3" title="VST3 Plug-in" description="For Ableton Live and most other DAWs.">
        <pkg-ref id="$ID_PREFIX.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit Plug-in" description="For Logic Pro, GarageBand, and Ableton Live (when Audio Units are enabled).">
        <pkg-ref id="$ID_PREFIX.au"/>
    </choice>
    <choice id="app" title="Standalone App" description="Runs Analog Drive on its own, without a DAW. Installed to Applications.">
        <pkg-ref id="$ID_PREFIX.app"/>
    </choice>
    <choice id="postinstall" visible="false">
        <pkg-ref id="$ID_PREFIX.postinstall"/>
    </choice>
    <pkg-ref id="$ID_PREFIX.vst3" version="$VERSION">vst3.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.au" version="$VERSION">au.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.app" version="$VERSION">app.pkg</pkg-ref>
    <pkg-ref id="$ID_PREFIX.postinstall" version="$VERSION">postinstall.pkg</pkg-ref>
</installer-gui-script>
EOF

mkdir -p "$(dirname "$OUT_PKG")"
rm -f "$OUT_PKG"
productbuild --quiet \
  --distribution "$WORK/Distribution.xml" \
  --resources "$SCRIPT_DIR/installer/resources" \
  --package-path "$WORK/pkgs" \
  ${INSTALLER_SIGN_ID:+--sign "$INSTALLER_SIGN_ID"} \
  "$OUT_PKG"

echo "Built installer: $OUT_PKG"
