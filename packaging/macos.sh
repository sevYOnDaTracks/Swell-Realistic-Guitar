#!/bin/bash
# Usage: macos.sh <Release artefacts dir> <output .pkg>
set -euo pipefail
release=$(cd "$1" && pwd)
out="$2"
here=$(cd "$(dirname "$0")" && pwd)
version=1.0.0
stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
product='Swell Realistic guitar'
root="$stage/root"
mkdir -p "$root/Library/Audio/Plug-Ins/VST3" "$root/Library/Application Support/Swell/Realistic Guitar" "$stage/resources"
# VST3 always; AU and the standalone app when they were built.
targets=("VST3/$product.vst3:Library/Audio/Plug-Ins/VST3" "AU/$product.component:Library/Audio/Plug-Ins/Components" "Standalone/$product.app:Applications")
for t in "${targets[@]}"; do
    src="$release/${t%%:*}"; dir="$root/${t#*:}"
    [[ -e "$src" ]] || { [[ "$t" == VST3* ]] && { echo "Missing $src"; exit 1; }; continue; }
    mkdir -p "$dir"; destination="$dir/$(basename "$src")"
    ditto --norsrc --noextattr "$src" "$destination"
    find "$destination" -name '._*' -delete
    if [[ -n "${APP_IDENTITY:-}" ]]; then
        codesign --force --deep --options runtime --timestamp --sign "$APP_IDENTITY" "$destination"
    else
        codesign --force --deep --sign - "$destination"
    fi
    codesign --verify --strict "$destination"
done
install -m 644 "$here/README.txt" "$root/Library/Application Support/Swell/Realistic Guitar/README.txt"
cp "$here/README.txt" "$stage/resources/README.txt"
xattr -cr "$root" 2>/dev/null || true
export COPYFILE_DISABLE=1
pkgbuild --analyze --root "$root" "$stage/components.plist"
/usr/bin/python3 - "$stage/components.plist" <<'PY'
import plistlib, sys
path = sys.argv[1]
with open(path, 'rb') as f:
    components = plistlib.load(f)
for item in components:
    item['BundleIsRelocatable'] = False
with open(path, 'wb') as f:
    plistlib.dump(components, f)
PY
pkgbuild --root "$root" --component-plist "$stage/components.plist" --ownership recommended --install-location / --identifier audio.swell.realisticguitar.installer --version "$version" "$stage/instrument.pkg"
cat > "$stage/Distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
  <title>Swell Realistic Guitar $version</title>
  <readme file="README.txt" mime-type="text/plain"/>
  <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
  <domains enable_localSystem="true" enable_currentUserHome="false" enable_anywhere="false"/>
  <volume-check><allowed-os-versions><os-version min="12.0"/></allowed-os-versions></volume-check>
  <choices-outline><line choice="instrument"/></choices-outline>
  <choice id="instrument" visible="false"><pkg-ref id="audio.swell.realisticguitar.installer"/></choice>
  <pkg-ref id="audio.swell.realisticguitar.installer" version="$version">instrument.pkg</pkg-ref>
</installer-gui-script>
XML
sign_args=()
if [[ -n "${INSTALLER_IDENTITY:-}" ]]; then sign_args=(--sign "$INSTALLER_IDENTITY" --timestamp); fi
productbuild --distribution "$stage/Distribution.xml" --resources "$stage/resources" --package-path "$stage" ${sign_args[@]+"${sign_args[@]}"} "$out"
