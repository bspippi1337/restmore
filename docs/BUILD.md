# RESTMORE / Native build and verification

## Requirements

- CMake 3.21 or newer
- ISO C11 compiler (Clang, GCC, or MSVC with C11 support)
- libcurl headers and link library when `RESTMORE_NETWORK=ON`
- Git and network access to download raylib 5.5 when `RESTMORE_GUI=ON`

## Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/restmore-cli --help
```

On Debian-based Linux, install a C compiler, CMake and `libcurl4-openssl-dev`. On macOS use Xcode command-line tools, CMake, and an appropriate libcurl setup. For the GUI:

```bash
cmake -S . -B build-gui -DCMAKE_BUILD_TYPE=Release -DRESTMORE_GUI=ON
cmake --build build-gui --parallel
```

## Windows

Install Visual Studio C++ Build Tools, CMake and libcurl for the active architecture. vcpkg is one supported dependency route. Supply the CMake vcpkg toolchain path for your installation:

```powershell
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Android ARM64 test APK

The repository now has a reproducible **C-only NativeActivity application build**, using raylib's Android backend, the Android NDK and the platform-provided NativeActivity. CI packages an ARM64 APK with a bundled sample OpenAPI document and a generated debug signature. No Java/Kotlin application business logic is required.

Download the [v0.1.0-alpha.4 APK](https://github.com/bspippi1337/restmore/releases/tag/v0.1.0-alpha.4). Install with `adb install restmore-v0.1.0-alpha.4-android-arm64-debug.apk` or with the device's own package installer. Debug signing keys are ephemeral per CI run: uninstall a previous incompatible debug build before installing another one.

The APK is currently **offline**, with HTTPS crawling deliberately disabled. CI verifies NDK compilation, APK packaging, Android manifest and signature validity; runtime launch on a physical phone, external file access and soft-keyboard interaction are **not yet validated**. Alpha 4's C GUI also implements screen-density-aware Android scaling and touch/swipe panel interaction, subject to device-level validation. It is a technical preview, not an app-store release.

### Native Android build source

See `.github/workflows/android-gui.yml` for the NDK/CMake/native-packaging commands and `android/AndroidManifest.xml` for the NativeActivity integration. The core is built with `RESTMORE_NETWORK=OFF` and `RESTMORE_GUI=ON` for the current APK.

## iOS

The iOS ARM64 C11 **core library** is cross-compiled in CI, but a complete iOS raylib application target, packaged and signed IPA, runtime testing, and distribution profile have not been verified. An iOS application should not be claimed as shipped.

## Local-only parser tests

```bash
cmake -S . -B build-offline -DRESTMORE_NETWORK=OFF -DRESTMORE_TESTS=ON
cmake --build build-offline
ctest --test-dir build-offline --output-on-failure
```

## Safe execution mode

```bash
restmore-cli --help
restmore-cli --import openapi.json --out atlas
restmore-cli docs.example.org
restmore-cli https://docs.example.org --execute --authorized --max-pages 6 --max-depth 1 --delay-ms 3000 --out atlas
```

The final example is **illustrative** and requires explicit permission for the target. It only fetches linked documentation. Use a test domain you control and review generated results before operational use.

## Verification gates

1. Compile strict C11 with compiler warnings enabled on all major desktop targets.
2. Run regression tests with assertions active in Release builds.
3. Exercise local import/export fixtures and verify output reproducibility.
4. Add sanitizers, malformed-input tests and offline integration fixtures.
5. Verify actual binaries and GUI behavior on each supported OS.
6. Verify Pages rendering, keyboard navigation, mobile layouts and no remote requests from the simulation field.

Never label a build target `production ready` without passing its release gates.
