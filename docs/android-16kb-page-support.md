# Android 4 KiB and 16 KiB page-size support

The PCSX2 fastmem implementation stores the host page size as a compile-time
constant. EmuCoreX therefore builds two variants of the same ARM64 native core:

- `libemucore_4k.so` with `OVERRIDE_HOST_PAGE_SIZE=0x1000`;
- `libemucore_16k.so` with `OVERRIDE_HOST_PAGE_SIZE=0x4000`.

Both libraries use 16 KiB-compatible ELF alignment. At runtime,
`AndroidNativeCoreSelector` reads `_SC_PAGESIZE` and loads exactly the matching
library before any JNI entry point is called.

## Local debug builds

The normal Android Studio/Gradle debug build contains only the 4 KiB core. This
keeps daily development builds fast and small:

```powershell
.\gradlew.bat :app:assembleDebug
```

## Distribution APK and AAB

The standard Android Studio/Gradle build automatically compiles both native
cores and packages them into APK and AAB artifacts:

```powershell
.\gradlew.bat :app:assembleRelease
```

AGP builds the 4 KiB core through `externalNativeBuild`. The integrated
`buildEmucore16k` and `stageEmucore16k` Gradle tasks build the second core in an
isolated CMake directory and add it to the generated JNI source set before native
libraries are merged. No local script, manual staging, or special Gradle property
is required.

```text
app/build/outputs/apk/release/app-release.apk
app/build/outputs/bundle/release/app-release.aab
```

The ordinary Build and Generate Signed Bundle or APK actions therefore produce
universal 4 KiB + 16 KiB artifacts.
