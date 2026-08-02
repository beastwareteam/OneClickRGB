# Changelog

All notable changes to OneClickRGB are documented here.

## [v3.6.0]

### Fixed

- **Autostart never ran with administrator rights, so the RAM colour was never
  applied at logon.** The application is manifested as `requireAdministrator`
  (the PawnIO kernel driver for G.Skill RAM needs it), and Windows silently
  refuses to launch such programs from `HKCU\...\Run` or from the Startup
  folder. Both were used, so the app effectively did not start at logon at all
  and had to be launched manually as administrator every time. Autostart now
  registers a **scheduled task with "run with highest privileges"**, which is
  the supported way to get an elevated process at logon without a UAC prompt.
  Existing Run-key entries are migrated automatically on first start.
  `install.bat`, `install_manual.bat` and the Inno Setup installer were changed
  the same way; the uninstallers remove the task.

- **The Windows key stayed locked after applying a profile.** The keyboard's
  key-lock register was addressed at an absolute `0x14`, which is only correct
  while onboard profile 0 is active. On profile 1 or 2 the unlock landed in
  profile 0's block while the active profile kept its lock bit. The register is
  now addressed relative to the active profile, and the value is **read back
  after writing** so the status log reports whether the keyboard actually
  accepted the unlock.

- **`--dry-run` still talked to real hardware.** The flag only short-circuited
  `ApplyColors()`, so the startup `FullHIDReset()` reconfigured the devices
  anyway. Dry runs now swap in a logging HID/SMBus backend, which covers every
  protocol path by construction and prints the actual packet bytes to the
  status log.

- **The RAM lost the race against the PawnIO service at logon.** Opening the
  driver is now retried for a few seconds, but only when the driver is not yet
  running - a missing DLL fails immediately. Each failure mode reports a
  distinct, actionable message instead of a generic "driver not running".

- Profile names are validated. A name containing `..` or a path separator could
  previously write outside the profile directory.

- Corrupt or hand-edited profile files no longer throw; malformed values fall
  back to their defaults.

### Changed

- Device protocols (ASUS Aura, SteelSeries, EVision, G.Skill), profile storage
  and autostart moved out of the single 3700-line source file into `src/devices`,
  `src/profile.cpp` and `src/autostart.cpp`, behind a hardware abstraction layer
  in `src/hal`. The Win32 front end remains in `oneclick_rgb_complete.cpp`.

- The ASUS hardware layout cache carries a signature, so a cache written by an
  older version is discarded instead of being read as a mismatched struct.

### Added

- **Unit test suite** (88 tests) covering protocol bytes, the key-lock
  addressing, the autostart task definition, colour correction, profile
  round-trips and SMBus register sequences. Runs against fake backends, so no
  hardware and no administrator rights are needed: `run_tests.bat`.

- `CMakeLists.txt` for both the application and the tests.

### CI

- The `tests` and `build` workflows referenced a `CMakeLists.txt` and a `tests/`
  directory that did not exist, ran on the retired `ubuntu-20.04`, and built a
  Win32 application for Linux and macOS. They now build and test what is
  actually there, on `windows-latest`, plus a Linux job that runs the portable
  modules under ASan and UBSan.

- Test failures are no longer swallowed by `|| true`.
