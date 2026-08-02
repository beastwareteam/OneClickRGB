# Testing

## Running

```batch
run_tests.bat
```

Configures, builds and runs the suite. No RGB hardware and no administrator
rights are needed — every device is simulated.

Via CMake directly:

```batch
cmake -S . -B build_tests -DONECLICKRGB_BUILD_APP=OFF
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

## How it works

The device protocols do not call hidapi or PawnIO. They talk to two interfaces:

| Interface | File | Real backend | Test backend |
|-----------|------|--------------|--------------|
| `hal::IHidBackend` | `src/hal/hid_backend.h` | `HidapiBackend` | `fakes::FakeHidBackend` |
| `hal::ISmbusBackend` | `src/hal/smbus_backend.h` | `PawnIoBackend` | `fakes::FakeSmbusBackend` |

`FakeHidBackend` records every byte written and replays scripted responses, so a
test asserts on the exact packet a protocol produces. `FakeSmbusBackend`
emulates the ENE controller's register file, so the G.Skill sequence can be
verified down to individual registers — that device needs a kernel driver and
admin rights and can never run on CI.

`hal::IHidBackend::Sleep` is part of the interface for the same reason: the
fakes accumulate the delay instead of blocking, so the full suite runs in
milliseconds despite the protocols containing several seconds of timing.

There is a third backend, `DryRunHidBackend` (`src/hal/hid_backend_dryrun.h`),
used by the application's `--dry-run` flag. It reports all devices as present
and logs the packet bytes to the status log instead of transmitting them.

## What is covered

| Suite | File | Focus |
|-------|------|-------|
| `evision`, `evision_keylock` | `test_evision.cpp` | Packet framing, checksum, **key-lock addressing**, unlock read-back, profile selection |
| `aura_config`, `aura_packet`, `aura_transfer` | `test_asus_aura.cpp` | Config-table parsing, direct-mode packets, LED chunking, per-zone correction |
| `gskill`, `gskill_diagnostics` | `test_gskill_ram.cpp` | RBG byte order, register addressing, LED-count clamping, driver diagnostics |
| `steelseries` | `test_steelseries.cpp` | Zone bitmasks, interface selection, persist command |
| `profile`, `profile_names`, `profile_storage` | `test_profile.cpp` | Round-trip, corrupt input, path traversal, listing |
| `autostart_xml` | `test_autostart.cpp` | Scheduled-task definition |

### The two reported defects

Both fixes are pinned by tests that fail against the old behaviour:

- **Windows key stayed locked.** `evision_keylock` asserts that the unlock is
  written to `profile * 0x40 + 0x14` rather than a fixed `0x14`. Against the old
  code, `set_keyboard_always_clears_the_key_lock` fails whenever the keyboard
  reports an active profile other than 0 — which is exactly when users saw the
  problem. Further tests cover the read-back reporting `StillLocked`,
  `Unverified` and `WriteFailed`.

- **Autostart did not run elevated.** `autostart_xml` asserts
  `RunLevel=HighestAvailable`, an `AtLogOn` trigger, `InteractiveToken`, no
  execution time limit, and correct XML escaping. None of these are observable
  from inside the running application, which is why the defect went unnoticed:
  the app simply was never started at logon.

## Adding a test

```cpp
#include "../src/devices/your_device.h"
#include "fakes/fake_hid.h"
#include "test_framework.h"

TEST(your_suite, describes_the_expected_behaviour) {
    fakes::FakeHidBackend hid;
    hid.AddDevice("dev0", VID, PID, USAGE_PAGE);

    YourFunction(hid, /* ... */);

    REQUIRE(hid.WriteCount() > 0);
    CHECK_EQ((int)hid.Writes()[0].data[0], 0xEC);
}
```

Add the file to `TEST_SOURCES` in `CMakeLists.txt`.

`CHECK` records a failure and continues; `REQUIRE` aborts the current test.
`CHECK_BYTES(actual, expected, len)` reports the first differing offset together
with a hex dump of both buffers.

The harness is `tests/test_framework.h`, roughly 100 lines. It is deliberately
dependency-free: the project ships no package manager, and vendoring a full
framework for these assertions would add more build surface than it removes.
