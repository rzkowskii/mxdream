
[mxdreamv0.1-screencap-1.webm](https://github.com/user-attachments/assets/c9354f5f-a805-4a56-b92c-c669eb3768df)

### mxdream (very early macOS Dreamcast emulator build)
This is a very early, experimental macOS build of the lxdream Sega Dreamcast emulator, shared just for fun. Expect rough edges, poor performance, and lots of things that don’t work yet. Use at your own risk. BIOS/flash files are not included; you must provide your own and configure their paths in Preferences after launching the app. This project is unaffiliated with SEGA and provided under GPLv2.

### Build (macOS, Apple Silicon)
```bash
# Dependencies
brew install glib gettext pcre2 libpng libisofs

# Configure & build
./autogen.sh
mkdir build-arm64 && cd build-arm64
../configure --disable-gtkui
make -j

# Create an app bundle with bundled libraries
make macbundle

# Optional: create a DMG from the app bundle
hdiutil create -volname "mxdream" -srcfolder lxdream.app -ov -format UDZO mxdream.dmg
```
