# MacBook Pro 2009 Windows Trackpad

The original Apple trackpad driver delays dragging after two taps on Windows XP.
This project changes the driver so that you can drag an item as soon as your finger moves the required distance after the second touch.

The settings program lets you adjust tap times and the minimum finger movement for dragging.
Changes take effect when you click **Apply**.

If you use the source code, start at **Compile**.
If you use the release ZIP, extract all files and start at **Patch**.
Before you install this software, make sure that the Apple trackpad driver operates on XP.

## Source code

| Path | Function |
| --- | --- |
| `src/driver/runtime.asm` | Adds the drag function and adjustable settings to the driver |
| `src/settings/` | Reads and changes the driver settings |
| `src/patcher/` | Makes a copy of the driver with the patch applied |
| `tools/build-patch.c` | Combines the driver and extension, then creates the patch code |

## Compile

Download Boot Camp 3.2 for Windows 32 bit from [Apple](https://download.info.apple.com/Mac_OS_X/061-9538.20101122.ght54/BootCamp_3.2_32-bit.exe)
or the [Internet Archive](https://web.archive.org/web/20160711085723id_/http://supportdownload.apple.com/download.info.apple.com/Apple_Support_Area/Apple_Software_Updates/Mac_OS_X/downloads/061-9538.20101122.ght54/BootCamp_3.2_32-bit.exe).
Both links supply the same file.
See [Apple driver information](docs/apple-driver.md) to find the required AppleMTP.sys file in the package.
For a full Boot Camp installation, install these packages in order:

1. [Boot Camp 3.0](https://archive.org/download/bootcamp3/bootcamp3.iso) from the Internet Archive.
2. [Boot Camp 3.1 for Windows 32 bit](https://download.info.apple.com/Mac_OS_X/061-7856.20100210.BcSLt/BootCamp_3.1_32-bit.exe) from Apple.
3. Boot Camp 3.2 from the links above.

See Apple's requirements for [Boot Camp 3.1](https://support.apple.com/en-us/106555) and [Boot Camp 3.2](https://support.apple.com/en-us/106576).

Install Visual Studio C++ Build Tools with [Windows XP support (`v141_xp`)](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp).
These tools include the C compiler, the MASM assembler, and Windows SDK 7.1A.
Install Windows 10 SDK **10.0.19041.0** from the [Microsoft SDK downloads page](https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads#windows-10) to supply the C runtime libraries.

Open a command prompt in the project folder.
In this command, replace `C:\drivers\AppleMTP.sys` with the path to your original Apple driver file:

```bat
build.cmd /p:DriverPath="C:\drivers\AppleMTP.sys" /p:TargetUniversalCRTVersion=10.0.19041.0
```

The command uses SDK 10.0.19041.0 to compile programs for XP.
SDK 10.0.26100.0 requires Windows functions introduced after XP.
The `build.cmd` script finds MSBuild automatically.
To select another MSBuild installation, set `MSBUILD_EXE_PATH` to the full path of its `MSBuild.exe` file.

The command creates `bin/driver-patch.exe` and `bin/trackpad-settings.exe`.

After the build succeeds, run `release.cmd` on the build computer to create a release package.
This script uses the `tar` program with ZIP support. Current Windows versions include this program.
The script creates `dist/xp-trackpad-xp-x86.zip`.
This ZIP contains the compiled programs, scripts, instructions, and MIT license.

## Patch

After compilation, use `release.cmd` to make the release ZIP.
If you use the release ZIP, extract all files to a folder on XP.

Use AppleMTP.sys **3.1.0.10** from Boot Camp **3.2 x86**.
The patch program uses the file size and SHA-256 hash to identify this driver.
If `xp-trackpad.sys` already exists in the folder, move that file before you run the command.

Open a command prompt in the folder. Run:

```bat
bin\driver-patch.exe "%SystemRoot%\System32\drivers\AppleMTP.sys" xp-trackpad.sys
```

The program writes the driver with the patch applied to `xp-trackpad.sys`.

## Install

After the patch command succeeds, continue with installation.
Log on with the account that you use for the trackpad. This account must have administrator access.
From the same folder, run:

```bat
cscript //nologo install.vbs
```

Restart Windows.
Open **XP Trackpad Settings** on the desktop.

| Setting | Default value |
| --- | ---: |
| Maximum tap time | 250 ms |
| Time before the second touch | 300 ms |
| Minimum finger movement for dragging | 8 device counts |

**Maximum tap time** sets how long a touch can last and count as a tap.
**Time before the second touch** sets the time allowed between the first finger lift and the second touch.
**Minimum finger movement for dragging** sets how far your finger must move during the second touch.
Movement is measured in trackpad device counts.

Click **Apply** to use the values immediately and save them.
To use the default values, click **Default values**, then **Apply**.
When you log on, the program applies the saved values and then closes.

To restore the original Apple driver, run `restore.cmd` as an administrator.
Then restart Windows.
The program files and saved settings stay on the computer.
Before you install a newer version, run `restore.cmd`. Then restart Windows.

See [Apple driver information](docs/apple-driver.md) for the supported device IDs and required driver file.
See [Driver interface](docs/driver-interface.md) for technical details.

The [MIT license](LICENSE) applies to the project code.
