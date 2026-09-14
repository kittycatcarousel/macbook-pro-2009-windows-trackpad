# Apple driver and hardware

The supported configuration is Windows XP Professional SP3 x86 with these device IDs:

| Device | Hardware ID |
| --- | --- |
| Apple Multi-Touch Trackpad | `USB\VID_05AC&PID_0236&MI_01` |
| Apple Multi-Touch Mouse | `USB\VID_05AC&PID_0236&MI_02` |

Other device IDs and Windows versions require compatibility testing.

If this driver is installed on XP, copy `%SystemRoot%\System32\drivers\AppleMTP.sys` to the build computer.
Use that copy as the `DriverPath` input for `build.cmd`.

Download BootCamp_3.2_32-bit.exe from [Apple](https://download.info.apple.com/Mac_OS_X/061-9538.20101122.ght54/BootCamp_3.2_32-bit.exe)
or the [Internet Archive](https://web.archive.org/web/20160711085723id_/http://supportdownload.apple.com/download.info.apple.com/Apple_Support_Area/Apple_Software_Updates/Mac_OS_X/downloads/061-9538.20101122.ght54/BootCamp_3.2_32-bit.exe).
Both downloads have this SHA-256 hash:

```text
a21d61bcb190cf93b544c100f5f93ff72df5befbab32f4ab755f083cead275b6
```

The original driver is in this Boot Camp package:

```text
BootCamp_3.2_32-bit.exe
  BootCampUpdate32.msp
    BootCamp3200dToBootCamp3200/Binary.MultiTP_Bin
      AppleMTP.sys
```

The SYS file version is **3.1.0.10**. Its size is **29,824 bytes**.
The INF package version is **3.2.0.1**, dated **2010-10-05**.
The SHA-256 hash of AppleMTP.sys is:

```text
341b15069fc91879bddce5d0258dea08eedfc1855e67ff52d770c16702c89781
```

Use AppleMTP.sys from this package or from an installation of the same driver.
Use the SHA-256 hash to identify the exact driver bytes.
The [Apple download page](https://support.apple.com/en-us/106576) identifies the Boot Camp 3.2 32-bit update.

The installation script requires this existing setting for the account that runs it:

```text
HKCU\Software\Apple Inc.\Trackpad\Mode
```

The Apple trackpad software supplies this setting.
Installation stops if the setting is missing.
