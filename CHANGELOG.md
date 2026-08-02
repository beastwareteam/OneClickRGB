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

- **The edge LED effect selection sent the wrong mode.** The combo lists the
  effects as Static, Breathing, Wave, Spectrum, Off, while the protocol numbers
  them FREEZE=0, WAVE=1, SPECTRUM=2, BREATHING=3, STATIC=4, OFF=5. The list
  index was stored and transmitted unchanged, so every selection was shifted by
  one position - "Static" sent FREEZE and "Off" sent STATIC, which meant the
  edge lighting could not be switched off at all. The keyboard effect combo had
  the same defect in the reverse direction and showed the wrong entry.

- **The status log showed doubled entries and overwrote itself.** Colour presets
  from the tray and the hotkeys started two overlapping apply runs, each run
  discarded the previous log, and worker threads wrote to the edit control
  directly so out-of-order updates left stale text on screen.

- **Effect modes defaulted to a value the hardware does not implement.** A fresh
  installation stored `kbMode: 0` - not a keyboard mode at all - and
  `edgeMode: 0`, which is FREEZE rather than Static as the UI claimed. Defaults
  now come from the protocol constants, and a mode the device does not define is
  rejected when the settings are loaded instead of going out on the wire.

- **Profiles were written to the wrong folder.** `L"\profiles"` contains no valid
  escape sequence, so the separator vanished and profiles landed in
  `%APPDATA%\OneClickRGBprofiles` instead of the app's own directory.

- **The ASUS hardware cache could never be written.** `L"\asus_hw_config.bin"`
  begins with `\a` - the BEL control character - so the filename was invalid and
  every open failed silently, re-scanning the board on each start.

- **Display power notifications were never delivered.** The
  `GUID_CONSOLE_DISPLAY_STATE` literal listed only seven of the eight `Data4`
  bytes, producing a GUID no subsystem recognises. Registration failures for
  both power and session notifications are now reported.

- **A second instance could run alongside the first**, so the autostart task and
  a manual launch drove the same devices and both wrote `config.json`. A launch
  while an instance is running now brings that window to the front instead.

- Channels beyond the eighth stayed dark on boards that report more than eight
  Aura zones; the apply path now covers all sixteen the protocol addresses.

- Profile names and the last-used profile survive non-ASCII characters. Paths
  are passed to the filesystem as native wide strings rather than through the
  active code page, and names are converted as UTF-8 instead of being truncated
  one character at a time.

- Hotkeys that another application already owns are named in the status log
  instead of silently doing nothing.

- **The status log stayed empty.** Two causes: the pending-notification flag was
  latched by every line written before the window existed, and since it was only
  cleared while handling the redraw message it stayed set forever and no update
  was ever posted. Appending also used `EM_REPLACESEL`, which an edit control
  silently ignores while it is `ES_READONLY`.

- **Every start ran a full "resume" cycle.** Windows reports the current display
  state the moment a power notification is registered, and that report was
  treated as a wake-up: HID reset plus a re-apply on each launch. Only an
  off-to-on transition counts now. This surfaced once the notification GUID was
  corrected - before that the registration failed and nothing arrived at all.

- The edge zone said nothing at all when the keyboard was absent, while the main
  matrix reported "not found" - so a missing keyboard was indistinguishable from
  broken edge lighting.

- **The side (edge) lighting never took the colour or the mode.** Its parameter
  block was written correctly and then overwritten twice: the implementation
  wrote the same payload to `profile+0x1A`, `profile+0x15` and an absolute
  `0x1E` on the theory that a keyboard would ignore the offsets that do not
  apply to it. All three overlap - for profile 0 the block occupies `0x1B-0x24`
  while the extra writes cover `0x16-0x1F` and `0x1E-0x27` - so the correct
  parameters were immediately replaced by misaligned bytes. Every keyboard this
  application supports carries VID `0x3299` (SPC Gear / ENDORFY), whose edge
  parameters live at `0x1a-0x23` inside the active profile, and that single
  block is now the only thing written. Verified against the EVision V2 protocol
  as implemented in OpenRGB.

- The edge zone ignored the brightness and speed sliders: both were hard-coded
  at maximum. It carries its own copies of the two and now follows the UI.

- Effect modes stored in a **profile** were not validated, so a profile written
  before the mode mapping was fixed put a combo index on the wire - typically
  edge mode `0x00` (FREEZE), which the UI never offered.

- The status log left the previous text standing when scrolled. The control's
  client area is inset by four pixels for the rounded border, so the scroll blit
  and the repaint disagreed about where the text belonged and old lines stayed
  visible beside the new ones.

### Changed

- Device protocols (ASUS Aura, SteelSeries, EVision, G.Skill), profile storage
  and autostart moved out of the single 3700-line source file into `src/devices`,
  `src/profile.cpp` and `src/autostart.cpp`, behind a hardware abstraction layer
  in `src/hal`. The Win32 front end remains in `oneclick_rgb_complete.cpp`.

- The ASUS hardware layout cache carries a signature, so a cache written by an
  older version is discarded instead of being read as a mismatched struct.

### Added

- `--debug` flag. Startup tracing used to be unconditional and appended to a
  `debug.log` in whatever the current directory happened to be; it is now opt-in
  and writes to `%APPDATA%\OneClickRGB`.

- **Unit test suite** (95 tests) covering protocol bytes, the key-lock
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
