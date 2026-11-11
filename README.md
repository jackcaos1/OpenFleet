# OpenFleet

An open-source client for *Star Trek: Armada* (2000) for Android and Linux.
It reads the files of **your own, legally acquired installation** (for example from GOG) and re-implements the game on a platform-independent C++17 core. The goal is a faithful reproduction that is checked against the behaviour of the original.

> **Status:** early development (debug builds). Currently working: model viewer, map view with the original HUD, ship movement with the original flight model and pathfinding, combat with weapons and shields, original-style lighting (including projectile and nebula lights), a zoomed-out symbol view for ships and stations, and a basic menu system. Many parts of the game (campaign, AI, most menus, save games) are still missing.

## Important

- **No original game files are included.** No models, textures, sounds, maps, videos or executables. On first start you select the folder of your own installation (Android: Storage Access Framework; Linux: `--data <folder>`).
- **No copy-protection circumvention**, no cracks.
- The file formats were examined for interoperability and documented in our own words (`docs/`). No decompiler output is part of this repository.

## Legal

This is a non-commercial fan project. It is not affiliated with, endorsed by or sponsored by Paramount, Activision, Mad Doc Software or any other rights holder. *Star Trek* and all related names and marks are the property of their respective owners and are mentioned here only to describe compatibility.

## Architecture

- `engine/`: platform-independent core (file loaders, game logic, rendering code)
- `desktop/`: Linux client (SDL2, OpenGL ES)
- `android/`: Android client (NativeActivity + EGL, Gradle)
- `docs/`: documentation of file formats and findings (e.g. `docs/model_audit.md`)
- `tools/`: helper scripts and tests, including `tools/check_layering.py`, which enforces the separation between core and platform layers

See `CLAUDE.md` for project rules, goals and milestones.

## Building

### Linux (also the development build)

    sudo apt install cmake ninja-build libsdl2-dev libgles-dev
    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build
    ./build/desktop/fleetclient_desktop --data <game folder>

Headless check:

    ./build/desktop/fleetclient_desktop --data <game folder> --model Fcruise1 --frames 30 --screenshot out.ppm --size 800x480

Core and tests only (no GL/SDL):

    cmake -S . -B build -G Ninja -DFLEET_BUILD_GAME=OFF -DFLEET_BUILD_DESKTOP=OFF

### Android

Requirements: JDK 17, Android SDK 34 with NDK and CMake 3.22.1.

    cd android
    JAVA17_HOME=/path/to/jdk17 ANDROID_HOME=/path/to/sdk ./build.sh

The APK ends up in `android/app/build/outputs/apk/debug/`. Alternatively, open `android/` in Android Studio. Target: arm64-v8a or x86_64 with OpenGL ES 3.0.

### Small test data set

Instead of the whole installation you can copy a small subset for development:

    python3 -I tools/make_testset.py <game folder> work/testset

Options: `--models Fcruise1,Kbattle` for a custom selection, `--all-models` for all models.

## Verification status

The core is covered by unit tests and checked against the real game data (archive checksums, model parsing compared against an independent Python parser). Behaviour that is re-implemented from the original program (lighting, pathfinding, flight model) is verified against the original code and tracked in `docs/`. Anything not yet verified is marked as open there.

## License

The code is licensed under <GPLv3 / MIT, to be decided>. This does not apply to the game data.
