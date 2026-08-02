# Building OneClickRGB

## Quick Build

```batch
git clone https://github.com/beastwareteam/OneClickRGB.git
cd OneClickRGB
build_app.bat
```

Output: `build_app\OneClickRGB.exe`

To build without CMake:

```batch
build_native.bat
```

Output: `build\OneClickRGB.exe`

Both scripts locate Visual Studio themselves.

---

## Requirements

### Minimum
- **Windows 10/11** (x64)
- **Visual Studio Build Tools 2019 or 2022**
  - Download: https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
  - Select: "Desktop development with C++"
- **CMake 3.20+** for `build_app.bat` (a copy ships in `tools/cmake/`)

### All Dependencies Bundled
No manual installation needed:
- HIDAPI (`dependencies/hidapi/`)
- PawnIO (`dependencies/PawnIO/`)
- nlohmann/json (`dependencies/nlohmann/`)

---

## Build Options

### Option 1: CMake (recommended)

```batch
build_app.bat
```

Or directly:

```batch
cmake -S . -B build_app -DONECLICKRGB_BUILD_TESTS=OFF
cmake --build build_app --config Release
```

Options:

| Option | Default | Effect |
|--------|---------|--------|
| `ONECLICKRGB_BUILD_APP` | on (Windows) | The Win32 GUI |
| `ONECLICKRGB_BUILD_TESTS` | on | The unit test suite |

The build produces two targets: `oneclickrgb_core` (device protocols, profile
storage, autostart — no Win32 GUI, no hidapi) and `OneClickRGB` (the
application, which links the core).

### Option 2: Batch script, no CMake

```batch
build_native.bat
```

Compiles the same sources with a direct `cl.exe` invocation. If you add a
source file, add it to **both** `CMakeLists.txt` and the `SOURCE` list in
`build_native.bat`.

---

## Quality gate

```batch
quality.bat
```

This is what CI runs, and what should pass before a push. Four stages, each
stopping the run on failure:

| Stage | Checks |
|-------|--------|
| 1. Build tests | Core and tests compile at `/W4 /WX` - a warning fails |
| 2. Run tests | All unit tests pass |
| 3. Build app | The GUI compiles; the curated warning list below is errors |
| 4. Smoke test | The built binary is **started** and asserted on |

The smoke test (`tools\smoke_test.ps1`) exists because the unit tests never
launch the program, and every defect that reached a user was of that kind: the
app started a second copy of itself, wrote its files somewhere else, or shipped
without the DLL it needs. It runs the real binary in `--dry-run` and checks that
it starts and shows a window, that a second launch exits instead of running
alongside, that `--debug` writes to `%APPDATA%` and nowhere else, that the
stored effect modes are values the hardware implements, and that no path with a
lost separator or a control character in it appears.

These MSVC warnings are errors project-wide, each because it already cost a bug:

| Warning | What it catches |
|---------|-----------------|
| `C4129` | Unrecognised escape - `L"\profiles"` silently lost its separator |
| `C4456`–`C4459` | A declaration hiding another - a local `GUID_CONSOLE_DISPLAY_STATE` hid that the SDK constant was being shadowed by a malformed copy |
| `C4700` | Use of an uninitialised local |
| `C4715` | Not all control paths return a value |
| `C4834` | Discarded `[[nodiscard]]` result |

## Running the tests

```batch
run_tests.bat
```

Or:

```batch
cmake -S . -B build_tests -DONECLICKRGB_BUILD_APP=OFF
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

The tests substitute fake HID and SMBus backends for the real ones, so no RGB
hardware and no administrator rights are needed. See
[docs/TESTING.md](docs/TESTING.md).

The core library also compiles with GCC/Clang, which is what CI uses to run the
suite under ASan and UBSan. The application itself is Windows-only.

---

## Running without hardware

```batch
dryrun.bat
```

Starts the application with `--dry-run`. Every device is simulated: the
protocols run end to end and their packet bytes go to the status log instead of
onto the bus. Useful for UI work and for inspecting what a protocol actually
sends.

### Command line flags

| Flag | Effect |
|------|--------|
| `--dry-run` | Simulate every device; nothing is transmitted |
| `--no-apply` | Start without applying colours |
| `--minimized` | Start in the tray |
| `--foreground` | Force the window to the front (used by the self-restart) |
| `--debug` | Write startup tracing to `%APPDATA%\OneClickRGB\debug.log` |

Only one instance runs at a time. Starting a second one brings the running
window to the front and exits - the autostart task and a manual launch would
otherwise drive the same devices simultaneously.

---

## Build Output

```
build_app/
├── OneClickRGB.exe      Main application
└── ...                  CMake intermediates
```

For a runnable copy, place these next to the exe:

| File | Source |
|------|--------|
| `hidapi.dll` | `dependencies/hidapi/` |
| `PawnIOLib.dll` | `dependencies/PawnIO/` |
| `SmbusI801.bin` | `dependencies/PawnIO/modules/` |
| `icon.png` | `src/` |

---

## Project Structure

```
OneClickRGB/
├── src/
│   ├── oneclick_rgb_complete.cpp   Win32 front end (window, tray, hotkeys)
│   ├── hal/                        Hardware abstraction
│   │   ├── hid_backend.h             HID interface
│   │   ├── hid_backend_hidapi.*      Real backend
│   │   ├── hid_backend_dryrun.h      Logging backend (--dry-run)
│   │   ├── smbus_backend.*           SMBus interface
│   │   └── smbus_backend_pawnio.*    Real backend
│   ├── devices/                    Device protocols
│   │   ├── asus_aura.*
│   │   ├── evision.*
│   │   ├── gskill_ram.*
│   │   └── steelseries.*
│   ├── autostart.*                 Elevated scheduled task at logon
│   ├── profile.*                   Colour profile storage
│   ├── app_config.h                Unified settings
│   ├── channel_config.h            Per-zone colour correction
│   ├── themes.h                    Theme definitions
│   ├── modern_ui.h                 UI components
│   └── OneClickRGB.ico/rc          Resources
├── tests/                          Unit tests
│   └── fakes/                        Recording HID and SMBus backends
├── dependencies/                   HIDAPI, PawnIO, nlohmann/json (bundled)
├── portable/                       Distribution package
├── installer/OneClickRGB.iss       Inno Setup script
├── docs/
├── CMakeLists.txt
├── build_app.bat                   CMake build
├── build_native.bat                Direct cl.exe build
└── run_tests.bat                   Build and run tests
```

### Architecture note

Device protocols do not call hidapi or PawnIO directly; they talk to the
interfaces in `src/hal`. That indirection is what makes them testable — the
tests swap in backends that record every byte instead of transmitting it. If
you add a device, put the protocol in `src/devices/`, take an
`hal::IHidBackend&` or `hal::ISmbusBackend&` as a parameter, and it is testable
for free.

---

## Version numbers

A release version appears in four places, and they must agree:

| File | Field |
|------|-------|
| `src/oneclick_rgb_complete.cpp` | `APP_VERSION` / `APP_VERSION_A` |
| `src/OneClickRGB.rc` | `FILEVERSION`, `PRODUCTVERSION`, string block |
| `CMakeLists.txt` | `project(... VERSION ...)` |
| `installer/OneClickRGB.iss` | `MyAppVersion` |

---

## Creating a Distribution Package

The `portable/` folder holds the ready-to-distribute files:

```
portable/
├── OneClickRGB.exe
├── hidapi.dll
├── PawnIOLib.dll
├── SmbusI801.bin
├── icon.png
├── PawnIO_setup.exe
├── install.bat           Auto-install
├── install_manual.bat    Interactive install
├── uninstall.bat         Clean removal
└── README.txt
```

```batch
powershell Compress-Archive -Path portable\* -DestinationPath OneClickRGB_Portable.zip
```

Tagged pushes build and publish this automatically — see
`.github/workflows/release.yml`.

**Note on autostart:** the install scripts register a scheduled task with
"run with highest privileges" rather than a Startup-folder shortcut. The
application is manifested as `requireAdministrator`, and Windows silently
refuses to launch such programs from the Startup folder or from
`HKCU\...\Run`. Do not "simplify" this back to a shortcut — the RAM would stop
getting its colour at logon.

---

## Troubleshooting

### "cl is not recognized"
Run from **x64 Native Tools Command Prompt**, or use `build_app.bat` /
`build_native.bat`, which set the environment themselves.

### "Cannot find hidapi.lib"
Check `dependencies/hidapi/hidapi.lib` exists.

### "LNK2019: unresolved external"
Usually a source file added to `CMakeLists.txt` but not to `build_native.bat`,
or vice versa.

### Build works but app crashes on start
Ensure `hidapi.dll` is next to the exe.

### RAM control does nothing
Requires administrator rights and the PawnIO driver
(`portable/PawnIO_setup.exe`, run once). The status log names the exact cause.

---

## Compiler Warnings

The Win32 front end still produces warnings that predate the current structure
(unreferenced subclass parameters, `LRESULT`-to-`int` conversions, a few unused
locals). They are harmless but not yet cleaned up. New code in `src/devices`,
`src/hal`, `src/profile.cpp` and `src/autostart.cpp` builds warning-free at
`/W4` and should stay that way.

---

## Development

### Building a debug version

```batch
cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build_debug --config Debug
```

### Adding a device

1. Create `src/devices/your_device.{h,cpp}`, taking `hal::IHidBackend&` (or
   `hal::ISmbusBackend&`) and a `devices::StatusFn` rather than calling hidapi
   or PawnIO.
2. Add the `.cpp` to `oneclickrgb_core` in `CMakeLists.txt` and to `SOURCE` in
   `build_native.bat`.
3. Add `tests/test_your_device.cpp` and register it in `TEST_SOURCES`.
4. Call it from `ApplyColors()` in the front end.
