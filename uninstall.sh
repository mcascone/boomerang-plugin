#!/bin/bash
# Boomerang+ Uninstaller
# Removes all installed components of Boomerang+

set -e

echo "Boomerang+ Uninstaller"
echo "======================"
echo ""
echo "This will remove:"
echo "  - VST3 plugin from /Library/Audio/Plug-Ins/VST3/"
echo "  - AU plugin from /Library/Audio/Plug-Ins/Components/"
echo "  - Standalone app from /Applications/"
echo ""
read -p "Continue with uninstallation? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

echo ""
echo "Uninstalling Boomerang+..."

# Remove VST3
if [ -d "/Library/Audio/Plug-Ins/VST3/Boomerang+.vst3" ]; then
    echo "Removing VST3 plugin..."
    rm -rf "/Library/Audio/Plug-Ins/VST3/Boomerang+.vst3"
    echo "  ✓ VST3 removed"
else
    echo "  - VST3 not found (already removed or not installed)"
fi

# Remove AU
if [ -d "/Library/Audio/Plug-Ins/Components/Boomerang+.component" ]; then
    echo "Removing AU plugin..."
    rm -rf "/Library/Audio/Plug-Ins/Components/Boomerang+.component"
    echo "  ✓ AU removed"
else
    echo "  - AU not found (already removed or not installed)"
fi

# Remove Standalone
if [ -d "/Applications/Boomerang+.app" ]; then
    echo "Removing Standalone app..."
    rm -rf "/Applications/Boomerang+.app"
    echo "  ✓ Standalone removed"
else
    echo "  - Standalone not found (already removed or not installed)"
fi

# Clean up package receipts
echo "Cleaning up installer receipts..."
pkgutil --forget com.MCMusicWorkshop.Boomerang.vst3 2>/dev/null && echo "  ✓ VST3 receipt removed" || true
pkgutil --forget com.MCMusicWorkshop.Boomerang.au 2>/dev/null && echo "  ✓ AU receipt removed" || true
pkgutil --forget com.MCMusicWorkshop.Boomerang.standalone 2>/dev/null && echo "  ✓ Standalone receipt removed" || true

echo ""
echo "✓ Boomerang+ has been uninstalled successfully!"
echo ""
echo "Note: User presets and settings are preserved in:"
echo "  ~/Library/Audio/Presets/ (if any)"
echo ""
echo "To remove presets as well, delete them manually."
