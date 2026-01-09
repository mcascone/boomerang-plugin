#!/bin/bash
# Boomerang+ Installer Build Script
# Creates a customizable macOS .pkg installer for all plugin formats

set -e

# Extract version from CMakeLists.txt (single source of truth)
CMAKE_VERSION=$(grep "^project" CMakeLists.txt | grep -oE '[0-9]+(\.[0-9]+)+')
if [ -z "$CMAKE_VERSION" ]; then
    echo "Error: Could not extract version from CMakeLists.txt"
    exit 1
fi

# Convert CMake version (2.0.0.5) to release version (2.0.0-alpha-5)
# Format: MAJOR.MINOR.PATCH.BUILD -> MAJOR.MINOR.PATCH-alpha-BUILD
VERSION=$(echo "$CMAKE_VERSION" | sed 's/\([0-9]*\.[0-9]*\.[0-9]*\)\.\([0-9]*\)/\1-alpha-\2/')
IDENTIFIER="com.MCMusicWorkshop.Boomerang"
BUILD_DIR="build/Boomerang_artefacts"
OUTPUT_DIR="build/installer"
COMPONENT_PKGS="$OUTPUT_DIR/components"

echo "Building Boomerang+ Installer v${VERSION}"
echo "============================================"

# Ensure build exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build artifacts not found. Run ./build.sh first."
    exit 1
fi

# Clean and create directories
echo "Setting up installer directories..."
rm -rf "$OUTPUT_DIR"
mkdir -p "$COMPONENT_PKGS"

# Build VST3 component package
echo "Building VST3 component..."
VST3_ROOT="$OUTPUT_DIR/vst3-root"
VST3_SCRIPTS="$OUTPUT_DIR/vst3-scripts"
mkdir -p "$VST3_ROOT/Library/Audio/Plug-Ins/VST3"
mkdir -p "$VST3_SCRIPTS"
cp -R "$BUILD_DIR/VST3/Boomerang+.vst3" "$VST3_ROOT/Library/Audio/Plug-Ins/VST3/"

cat > "$VST3_SCRIPTS/postinstall" << 'EOF'
#!/bin/bash
xattr -cr "/Library/Audio/Plug-Ins/VST3/Boomerang+.vst3" 2>/dev/null || true
exit 0
EOF
chmod +x "$VST3_SCRIPTS/postinstall"

pkgbuild --root "$VST3_ROOT" \
         --identifier "${IDENTIFIER}.vst3" \
         --version "$VERSION" \
         --scripts "$VST3_SCRIPTS" \
         --install-location "/" \
         "$COMPONENT_PKGS/VST3.pkg"

# Build AU component package
echo "Building AU component..."
AU_ROOT="$OUTPUT_DIR/au-root"
AU_SCRIPTS="$OUTPUT_DIR/au-scripts"
mkdir -p "$AU_ROOT/Library/Audio/Plug-Ins/Components"
mkdir -p "$AU_SCRIPTS"
cp -R "$BUILD_DIR/AU/Boomerang+.component" "$AU_ROOT/Library/Audio/Plug-Ins/Components/"

cat > "$AU_SCRIPTS/postinstall" << 'EOF'
#!/bin/bash
xattr -cr "/Library/Audio/Plug-Ins/Components/Boomerang+.component" 2>/dev/null || true
exit 0
EOF
chmod +x "$AU_SCRIPTS/postinstall"

pkgbuild --root "$AU_ROOT" \
         --identifier "${IDENTIFIER}.au" \
         --version "$VERSION" \
         --scripts "$AU_SCRIPTS" \
         --install-location "/" \
         "$COMPONENT_PKGS/AU.pkg"

# Build Standalone component package
echo "Building Standalone component..."
STANDALONE_ROOT="$OUTPUT_DIR/standalone-root"
STANDALONE_SCRIPTS="$OUTPUT_DIR/standalone-scripts"
mkdir -p "$STANDALONE_ROOT/Applications"
mkdir -p "$STANDALONE_SCRIPTS"
cp -R "$BUILD_DIR/Standalone/Boomerang+.app" "$STANDALONE_ROOT/Applications/"
cp uninstall.sh "$STANDALONE_ROOT/Applications/Uninstall Boomerang+.command"
chmod +x "$STANDALONE_ROOT/Applications/Uninstall Boomerang+.command"

cat > "$STANDALONE_SCRIPTS/postinstall" << 'EOF'
#!/bin/bash
xattr -cr "/Applications/Boomerang+.app" 2>/dev/null || true
xattr -cr "/Applications/Uninstall Boomerang+.command" 2>/dev/null || true
exit 0
EOF
chmod +x "$STANDALONE_SCRIPTS/postinstall"

# Create component plist to prevent bundle relocation
COMP_PLIST="$OUTPUT_DIR/component.plist"
cat > "$COMP_PLIST" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<array>
    <dict>
        <key>BundleIsRelocatable</key>
        <false/>
        <key>BundleIsVersionChecked</key>
        <false/>
        <key>BundleHasStrictIdentifier</key>
        <false/>
        <key>RootRelativeBundlePath</key>
        <string>Applications/Boomerang+.app</string>
    </dict>
</array>
</plist>
EOF

pkgbuild --root "$STANDALONE_ROOT" \
         --identifier "${IDENTIFIER}.standalone" \
         --version "$VERSION" \
         --scripts "$STANDALONE_SCRIPTS" \
         --install-location "/" \
         --component-plist "$COMP_PLIST" \
         "$COMPONENT_PKGS/Standalone.pkg"

# Create distribution XML for custom installer
echo "Creating distribution definition..."
cat > "$OUTPUT_DIR/distribution.xml" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Boomerang+ ${VERSION}</title>
    <organization>com.MCMusicWorkshop</organization>
    <domains enable_localSystem="true"/>
    <options customize="always" require-scripts="false" rootVolumeOnly="true" />
    
    <welcome file="welcome.html" mime-type="text/html" />
    
    <choices-outline>
        <line choice="vst3.choice"/>
        <line choice="au.choice"/>
        <line choice="standalone.choice"/>
    </choices-outline>
    
    <choice id="vst3.choice" title="VST3 Plugin" description="Install VST3 plugin for DAW use" start_selected="true">
        <pkg-ref id="${IDENTIFIER}.vst3"/>
    </choice>
    
    <choice id="au.choice" title="Audio Unit Plugin" description="Install AU plugin for Logic Pro, GarageBand, and other AU hosts" start_selected="true">
        <pkg-ref id="${IDENTIFIER}.au"/>
    </choice>
    
    <choice id="standalone.choice" title="Standalone Application" description="Install standalone app that runs independently (includes uninstaller)" start_selected="true">
        <pkg-ref id="${IDENTIFIER}.standalone"/>
    </choice>
    
    <pkg-ref id="${IDENTIFIER}.vst3" version="${VERSION}" auth="root">components/VST3.pkg</pkg-ref>
    <pkg-ref id="${IDENTIFIER}.au" version="${VERSION}" auth="root">components/AU.pkg</pkg-ref>
    <pkg-ref id="${IDENTIFIER}.standalone" version="${VERSION}" auth="root">components/Standalone.pkg</pkg-ref>
</installer-gui-script>
EOF

# Create welcome HTML
cat > "$OUTPUT_DIR/welcome.html" << EOF
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; padding: 20px; }
        h1 { font-size: 24px; margin-bottom: 10px; }
        p { font-size: 14px; line-height: 1.5; }
    </style>
</head>
<body>
    <h1>Boomerang+ Audio Looper</h1>
    <p>Version ${VERSION}</p>
    <p>This installer will guide you through installing Boomerang+ on your Mac.</p>
    <p>You can customize which components to install on the next screen.</p>
    <p><strong>Installation locations:</strong></p>
    <ul>
        <li>VST3: /Library/Audio/Plug-Ins/VST3/</li>
        <li>AU: /Library/Audio/Plug-Ins/Components/</li>
        <li>Standalone: /Applications/</li>
    </ul>
</body>
</html>
EOF

# Build the product package
echo "Building product installer..."
productbuild --distribution "$OUTPUT_DIR/distribution.xml" \
             --package-path "$COMPONENT_PKGS" \
             --resources "$OUTPUT_DIR" \
             "$OUTPUT_DIR/Boomerang-${VERSION}.pkg"

echo ""
echo "✓ Customizable installer created: $OUTPUT_DIR/Boomerang-${VERSION}.pkg"
echo ""
echo "The installer allows users to choose:"
echo "  ☑ VST3 Plugin (default: enabled)"
echo "  ☑ AU Plugin (default: enabled)"
echo "  ☑ Standalone App (default: enabled)"
echo ""
echo "File size:"
du -h "$OUTPUT_DIR/Boomerang-${VERSION}.pkg"
