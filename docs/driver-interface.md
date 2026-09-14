# Driver interface

The assembly extension operates inside AppleMTP.sys.
The settings program opens `\\.\AppleTrackpad` and sends these requests with `DeviceIoControl`:

| IOCTL | Operation | Buffer |
| --- | --- | --- |
| `0xF2010` | Read runtime settings | Output: at least 16 bytes; returns 16 bytes |
| `0xF2014` | Write runtime settings | Input: exactly 16 bytes |

The buffer contains four little-endian DWORDs:

```c
struct TimingV2 {
    uint32_t version;       /* Set to 2. */
    uint32_t tap_ms;
    uint32_t second_touch_ms;
    uint32_t movement_counts;
};
```

Each setting accepts the full unsigned 32-bit range.
Incorrect write sizes and versions return `STATUS_INVALID_PARAMETER`.
Requests `0xF2008` and `0xF200C` return `STATUS_REVISION_MISMATCH`.
Apple mode requests `0xF2000` and `0xF2004` stay available.

Each setting uses an aligned DWORD store.
Input processing can run between the three stores.
An input packet can use a mixture of previous and new values during a settings change.
The program reads the live values after a write, then saves them in:

```text
HKCU\Software\XPTrackpadSettings\TimingV2
```

This `REG_BINARY` value contains the same 16-byte buffer.
The `--apply-saved` command applies it in the background, then closes.
If a save operation fails, the program reports that the settings are active and gives the registry error.

## Patch construction

MSBuild assembles `runtime.asm` with its MASM build task.
The C tool in `tools/build-patch.c` links its code at RVA `0x7480`.
It resolves the COFF symbols and relocations against addresses in the specified Apple driver.
It installs six hooks, extends the last PE section, rebuilds the relocation table, and calculates the PE checksum.

| File offset | Entry point |
| --- | --- |
| `0x05F3` | `runtime_dispatch` |
| `0x2758` | `runtime_tap` |
| `0x28E2` | `runtime_gap` |
| `0x294F` | `runtime_motion` |
| `0x645F` | `runtime_init` |
| `0x27F9` | `runtime_reset` |

The patch increases the device context size from `0x18D0` to `0x18F0`.
The interface version and settings use device context offsets `0x18D0` through `0x18DC`.
Signed 64-bit movement accumulators use device context offsets `0x18E0` and `0x18E8`.
The driver enables dragging when absolute net movement on either axis is at or above the threshold.
The timer comparisons use unsigned elapsed milliseconds.

The C build tool generates copy operations for the changed bytes.
The XP patch program applies these operations to a copy of the specified original driver.
