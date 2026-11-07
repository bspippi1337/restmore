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

## Android / iOS

The core is written in C11, but **building an executable is not the same as shipping a native app**.

- Android requires the Android NDK, an appropriate libcurl build, raylib integration for Android, a Java/Kotlin or fully native app bootstrap, APK/AAB packaging, manifest, permissions and signing.
- iOS requires a platform-specific CMake/Xcode toolchain, libcurl/raylib integration, an app entrypoint, signing and installation/distribution provisioning.
- Platform runtime smoke tests and a signed installable release must pass before claiming support.

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
