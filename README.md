# AgIsoStack DDOP Generator

## Overview

This program allows you to create, save, load, and edit ISO11783 device descriptor object pools (DDOP) for use with communicating with an ISOBUS task controller.

It is written in C++ and is based on the DDOP objects from [AgIsoStack++](https://github.com/Open-Agriculture/AgIsoStack-plus-plus), with a simple GUI created with [Dear ImGui](https://github.com/ocornut/imgui) on SDL2 and OpenGL 3.

When used in combination with AgIsoStack (or any other TC client), it provides an easy way to visualize your DDOP, build a binary version of it, and easily deserialize it back into C++ objects if you want to interact with a DDOP in your own application.

![Main Screen Image](docs/images/example1.png)

### Features

* Supports dynamically editing any DDOP or creating one from scratch
* Compatible with both TC version 3 and 4, and detects which one a file uses when opening it
* Opens a DDOP passed on the command line, so it can be registered as your `.ddop` handler
* Basic object pool error checking to help you find errors before loading onto a TC
* Saves DDOPs as `.ddop` by default and still opens `.iop` DDOPs from 1.2.0 and earlier (the extension is a convention; ISO 11783 only defines the binary layout)
* Exports the same pool as an ISOXML `TASKDATA.XML`
* Looks each DDI up in the ISO 11783-11 data dictionary and shows its name next to the number
* Completely free and open source alternative to many paid products!

### Using it

**File → New** starts an empty DDOP and asks for your device information; **File → Open** loads an existing one. `EXAMPLE.ddop` and `EXAMPLE_V4.ddop` in this repository are there to open and poke at.

The left pane is the object tree, and whatever you select in it is edited in the right pane. **Create Object** adds a device element, process data, property or value presentation, and a device element gains children by referencing objects that already exist.

**Edit → Check for Errors** serializes the pool and reports what a task controller would reject, including device elements that share an element number. **File → Save** writes the binary DDOP, and **File → Export as ISOXML** writes the same pool as a TASKDATA.XML.

### TC version 3 or 4

A DDOP file does not record which task controller version it was built for. **File → Open** asks which version to read it as, and **Save As** asks which version to write.

Both versions encode every object the same way, except the device object:

|  | Version 3 | Version 4 |
|---|---|---|
| Device object | ends after the localization label | adds an extended structure label (up to 32 bytes) |
| Designators | up to 32 bytes | up to 128 bytes |

ISO 11783-10 caps a designator at 32 characters in either version, and this editor enforces neither limit. Saving at version 3 does not shorten a designator you typed while editing at version 4, so keep them short yourself. If a file will not open at the version you picked, open it again at the other one.

Save at version 3 unless every TC you target is version 4. AgIsoStack lowers the version to match the server only when your client hands it the deserialized pool object; a client that uploads the saved bytes sends them unchanged, and a version 3 TC does not expect the extra device object bytes.

### Releases

Prebuilt binaries for Windows, Linux and macOS 11+ (Apple Silicon) are attached to every [release](https://github.com/Open-Agriculture/AgIsoDDOPGenerator/releases).

The Linux binary links SDL2 and OpenGL dynamically and does not bundle them:

```
sudo apt install libsdl2-2.0-0 libopengl0
```

The Linux archive also carries a desktop entry, a MIME definition and the application icon. Install
them to open a DDOP by double-clicking it:

```
sudo install -Dm644 agisoddopgenerator.desktop /usr/share/applications/agisoddopgenerator.desktop
sudo install -Dm644 agisoddopgenerator.xml /usr/share/mime/packages/agisoddopgenerator.xml
sudo install -Dm644 agisoddopgenerator.svg /usr/share/icons/hicolor/scalable/apps/agisoddopgenerator.svg
sudo install -Dm644 agisoddopgenerator.svg /usr/share/icons/hicolor/scalable/mimetypes/application-x-iso11783-ddop.svg
sudo install -Dm755 AgIsoDDOPGenerator /usr/local/bin/AgIsoDDOPGenerator
sudo update-mime-database /usr/share/mime
sudo update-desktop-database /usr/share/applications
sudo gtk-update-icon-cache /usr/share/icons/hicolor
xdg-mime default agisoddopgenerator.desktop application/x-iso11783-ddop
```

The second icon copy is the one file managers look up by MIME type. The last four commands come from
the `shared-mime-info`, `desktop-file-utils`, `libgtk-3-bin` and `xdg-utils` packages.

`cmake --install` installs the same files under `${CMAKE_INSTALL_PREFIX}/share` and does not refresh
the caches.

A `.ddop` file is recognized by its name. A `.iop` DDOP saved by an earlier version is recognized by
its `DVC` header instead, because
[AgIsoVirtualTerminal](https://github.com/Open-Agriculture/AgIsoVirtualTerminal) pools use that
extension too. Pools starting with another object are not recognized.

Windows and macOS have no installer or app bundle yet, so pass the file on the command line.

The macOS binary is unsigned, so Gatekeeper quarantines it on download. Keep it in the same folder as the `libSDL2-2.0.0.dylib` from the same archive, and clear the quarantine flag:

```
xattr -dr com.apple.quarantine AgIsoDDOPGenerator
```

### Compilation

This project is built with CMake.

Make sure you have all the dependencies installed.

```
sudo apt install cmake build-essential libgl1-mesa-dev libxext-dev
```

Clone the repo. The submodules are not optional - if you already cloned without them, run `git submodule update --init --recursive`:

```
git clone https://github.com/Open-Agriculture/AgIsoDDOPGenerator.git --recurse-submodules
```

Then compile with CMake:

```
cd AgIsoDDOPGenerator
cmake -S . -B build
cmake --build build
```

Then run `./build/AgIsoDDOPGenerator`.

### Community

Bug reports and feature requests belong in [Issues](https://github.com/Open-Agriculture/AgIsoDDOPGenerator/issues). For everything else, the Open-Agriculture community is on [Discord](https://discord.gg/uU2XMVUD4b) and [Telegram](https://t.me/+kzd4-9Je5bo1ZDg6).

### License

MIT, see [LICENSE](LICENSE). Contributions are welcome under the [code of conduct](CODE_OF_CONDUCT.md).
