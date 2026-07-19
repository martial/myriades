#!/bin/bash
set -e

# ========================================================================
#  Myriades — Release Build, Sign, Notarize & Package
# ========================================================================
#
#  PREREQUISITES (one-time setup):
#    1. Install a "Developer ID Application" certificate:
#       https://developer.apple.com/account/resources/certificates/list
#
#    2. Store notarization credentials in Keychain:
#       xcrun notarytool store-credentials "HUYGHE_BALE_NOTARY" \
#           --apple-id "your@email.com" \
#           --team-id "PC5K6NYDF2" \
#           --password "xxxx-xxxx-xxxx-xxxx"
#
#  USAGE:
#    ./build_release.sh                    # universal build + sign + notarize + DMG
#    ./build_release.sh --skip-notarize    # build + sign only (faster)
#    ./build_release.sh --arm64-only       # skip the x86_64 pass
# ========================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
APP_NAME="Myriades"
BIN_NAME="myriades"
BUNDLE_ID="com.martial.myriades"
VERSION="${VERSION:-1.0.0}"
DIST_DIR="$SCRIPT_DIR/dist"
APP_PATH="$DIST_DIR/$APP_NAME.app"
ENTITLEMENTS="$SCRIPT_DIR/of.entitlements"
DMG_PATH="$DIST_DIR/$APP_NAME-$VERSION.dmg"

SIGN_IDENTITY="Developer ID Application: Martial Geoffre Rouland (PC5K6NYDF2)"
NOTARY_PROFILE="HUYGHE_BALE_NOTARY"

# The Xcode project resolves OF via the relative OF_PATH = ../../.., so the
# build runs from a copy of the repo placed inside the OF tree (same as CI).
OF_ROOT="$(sed -n 's/^OF_ROOT *= *//p' "$SCRIPT_DIR/config.make" | head -1)"
BUILD_AREA="$OF_ROOT/apps/myApps/myriades-release"

SKIP_NOTARIZE=0
ARCHS="arm64 x86_64"
for arg in "$@"; do
    case "$arg" in
        --skip-notarize) SKIP_NOTARIZE=1 ;;
        --arm64-only)    ARCHS="arm64" ;;
    esac
done

if [ ! -d "$OF_ROOT/libs/openFrameworksCompiled" ]; then
    echo "ERROR: OF_ROOT not found or invalid: $OF_ROOT (set it in config.make)"
    exit 1
fi

# ─── Validate signing identity ──────────────────────────────────────
if ! security find-identity -v -p codesigning | grep -q "$SIGN_IDENTITY"; then
    echo "ERROR: Signing identity not found in keychain:"
    echo "  $SIGN_IDENTITY"
    echo ""
    echo "Available identities:"
    security find-identity -v -p codesigning
    exit 1
fi

echo "========================================"
echo "  Myriades — Release Builder"
echo "========================================"
echo ""
echo "  Version:  $VERSION"
echo "  Identity: $SIGN_IDENTITY"
echo "  Archs:    $ARCHS"
echo "  Notarize: $([ $SKIP_NOTARIZE -eq 0 ] && echo 'yes' || echo 'skip')"
echo ""

mkdir -p "$DIST_DIR"
TMP_BIN_DIR="$DIST_DIR/tmp_bin"
rm -rf "$TMP_BIN_DIR"
mkdir -p "$TMP_BIN_DIR"

# ─── 1. Build Release binary (xcodebuild, universal) ────────────────
echo "=== Copying project into OF tree ==="
mkdir -p "$BUILD_AREA"
rsync -a --delete \
    --exclude .git --exclude dist --exclude obj --exclude build \
    --exclude "bin/$BIN_NAME.app" \
    "$SCRIPT_DIR/" "$BUILD_AREA/"

echo "=== Building Release ($ARCHS) ==="
xcodebuild -project "$BUILD_AREA/$BIN_NAME.xcodeproj" \
    -target "$BIN_NAME" -configuration Release \
    ARCHS="$ARCHS" ONLY_ACTIVE_ARCH=NO build | tail -5
test "${PIPESTATUS[0]}" -eq 0

cp "$BUILD_AREA/bin/$BIN_NAME.app/Contents/MacOS/$BIN_NAME" "$TMP_BIN_DIR/$BIN_NAME"
lipo -info "$TMP_BIN_DIR/$BIN_NAME"
echo ""

# ─── 2. Assemble .app bundle ────────────────────────────────────────
echo "=== Assembling .app bundle ==="
rm -rf "$APP_PATH"
mkdir -p "$APP_PATH/Contents/MacOS"
mkdir -p "$APP_PATH/Contents/Resources"

cp "$TMP_BIN_DIR/$BIN_NAME" "$APP_PATH/Contents/MacOS/$BIN_NAME"

# Icon (openFrameworks default, copied into the bundle by the Xcode build)
if [ -f "$BUILD_AREA/bin/$BIN_NAME.app/Contents/Resources/of.icns" ]; then
    cp "$BUILD_AREA/bin/$BIN_NAME.app/Contents/Resources/of.icns" \
       "$APP_PATH/Contents/Resources/of.icns"
fi

# Seed data inside the bundle so a bare .app still works (the app prefers an
# external data/ folder next to the .app when one exists — see ofApp::setup)
rsync -a "$SCRIPT_DIR/bin/data/" "$APP_PATH/Contents/Resources/data/"

# Write Info.plist
cat > "$APP_PATH/Contents/Info.plist" << PLIST_EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
  "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>Myriades — HourGlass LED Control</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundleExecutable</key>
    <string>${BIN_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>of.icns</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.5</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSSupportsAutomaticGraphicsSwitching</key>
    <true/>
    <key>NSCameraUsageDescription</key>
    <string>This app needs to access the camera</string>
    <key>NSMicrophoneUsageDescription</key>
    <string>This app needs to access the microphone</string>
    <key>NSHumanReadableCopyright</key>
    <string>Martial Geoffre-Rouland 2026</string>
</dict>
</plist>
PLIST_EOF

echo "  App bundle: $APP_PATH"
echo ""

# ─── 3. Sign ────────────────────────────────────────────────────────
echo "=== Signing app (hardened runtime) ==="

echo "  Signing executable..."
codesign --force --sign "$SIGN_IDENTITY" --timestamp --options runtime \
    --entitlements "$ENTITLEMENTS" \
    "$APP_PATH/Contents/MacOS/$BIN_NAME"

echo "  Signing app bundle..."
codesign --force --sign "$SIGN_IDENTITY" --timestamp --options runtime \
    --entitlements "$ENTITLEMENTS" "$APP_PATH"

echo "  Verifying signature..."
codesign --verify --deep --strict --verbose=2 "$APP_PATH"
echo ""

# ─── 4. Notarize ────────────────────────────────────────────────────
if [ $SKIP_NOTARIZE -eq 0 ]; then
    echo "=== Notarizing app (this may take a few minutes) ==="

    ZIP_PATH="$DIST_DIR/$APP_NAME.zip"
    ditto -c -k --keepParent "$APP_PATH" "$ZIP_PATH"

    xcrun notarytool submit "$ZIP_PATH" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait

    xcrun stapler staple "$APP_PATH"

    rm -f "$ZIP_PATH"

    echo ""
    echo "=== Verifying notarization ==="
    spctl --assess --type execute -vvv "$APP_PATH"
    echo ""
else
    echo "=== Skipping notarization (--skip-notarize) ==="
    echo ""
fi

# ─── 5. Create DMG (app + data side by side) ────────────────────────
echo "=== Creating DMG ==="
rm -f "$DMG_PATH"
DMG_STAGING="$DIST_DIR/dmg_staging"
rm -rf "$DMG_STAGING"
mkdir -p "$DMG_STAGING"
cp -a "$APP_PATH" "$DMG_STAGING/"
rsync -a "$SCRIPT_DIR/bin/data/" "$DMG_STAGING/data/"
hdiutil create -volname "$APP_NAME" -srcfolder "$DMG_STAGING" \
    -ov -format UDZO "$DMG_PATH"
rm -rf "$DMG_STAGING"

if [ $SKIP_NOTARIZE -eq 0 ]; then
    echo "  Notarizing DMG..."
    xcrun notarytool submit "$DMG_PATH" \
        --keychain-profile "$NOTARY_PROFILE" \
        --wait
    xcrun stapler staple "$DMG_PATH"
fi

rm -rf "$TMP_BIN_DIR"
echo ""

# ─── Done ────────────────────────────────────────────────────────────
echo "========================================"
echo "  Build complete!"
echo "========================================"
echo ""
echo "  App: $APP_PATH"
echo "  DMG: $DMG_PATH"
echo "  Size: $(du -sh "$DMG_PATH" | cut -f1)"
if [ $SKIP_NOTARIZE -eq 0 ]; then
    echo "  Signed and notarized."
fi
echo ""
echo "  DMG contains $APP_NAME.app + data/ folder. Copy both side by side"
echo "  to keep settings editable outside the signed bundle."
echo ""
