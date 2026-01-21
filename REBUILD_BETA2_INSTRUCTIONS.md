# Instructions to Rebuild v2.0.0-beta-2 Packages

## Issue
The v2.0.0-beta-2 packages may be corrupt - Intel Mac install failed.

## Investigation Results
- Original build workflow (run 21106584279) completed successfully
- Universal binary built with correct cmake flags: `-DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"`
- All validation passed (pluginval on macOS, Windows, Linux)
- No obvious build process errors
- Ad-hoc code signing was applied correctly in CI

## Root Cause
The packages themselves appear to have been built correctly, but may have been corrupted during upload/download or have signing issues on Intel Macs.

## Solution: Rebuild and Replace Packages

### Option 1: Re-trigger via Tag (Recommended)

1. Delete the remote tag:
   ```bash
   git push --delete origin v2.0.0-beta-2
   ```

2. Delete the local tag:
   ```bash
   git tag -d v2.0.0-beta-2
   ```

3. Re-create the tag on the same commit (or latest main if you want to include fixes):
   ```bash
   git tag v2.0.0-beta-2 1f3f23e  # or $(git rev-parse HEAD) for current commit
   ```

4. Push the tag to trigger the workflow:
   ```bash
   git push origin v2.0.0-beta-2
   ```

5. The workflow will:
   - Build all packages fresh
   - Run validation
   - Create a draft release
   - Attach all assets

6. Edit the draft release notes and publish

### Option 2: Trigger Workflow Manually

1. Go to: https://github.com/mcascone/boomerang-plugin/actions/workflows/build.yml

2. Click "Run workflow" button

3. Select branch: `main`

4. Click "Run workflow"

5. Wait for build to complete

6. Download artifacts from the workflow run

7. Manually upload to the v2.0.0-beta-2 release page:
   - Delete old assets
   - Upload new assets:
     - Boomerang-macOS.zip
     - Boomerang-2.0.0-beta-2.pkg
     - Boomerang-Windows.zip
     - Boomerang-Linux.tar.gz

### Option 3: Build Locally on macOS

If you have a Mac with Xcode:

```bash
# Clone repo
git clone --recursive https://github.com/mcascone/boomerang-plugin.git
cd boomerang-plugin
git checkout v2.0.0-beta-2

# Build
./build.sh

# Create installer
./build-installer.sh

# Package for distribution
cd build/Boomerang_artefacts/Release
zip -r ../../../Boomerang-macOS.zip VST3 AU Standalone
cd ../../..

# Packages will be in:
# - build/installer/Boomerang-2.0.0-beta-2.pkg
# - Boomerang-macOS.zip
```

Then manually upload to the release page.

## Verification

After rebuilding, verify the packages:

1. Download the fresh Boomerang-2.0.0-beta-2.pkg
2. Install on an Intel Mac
3. Verify the plugin loads in a DAW (Logic Pro, Ableton, etc.)
4. Check the architectures:
   ```bash
   lipo -info "/Library/Audio/Plug-Ins/VST3/Boomerang+.vst3/Contents/MacOS/Boomerang+"
   ```
   Should show: `Architectures in the fat file: x86_64 arm64`

## Next Steps

1. Choose one of the above options
2. Rebuild the packages
3. Replace the assets on the v2.0.0-beta-2 release
4. Delete this instruction file
5. Notify users of the fix (optional release note update)
