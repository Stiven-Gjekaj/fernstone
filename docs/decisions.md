# Decisions

This file records each choice of the owner and each technical decision.
Each entry gives the reason. The owner made these choices on 2026-10-24.

## What the app is

- **Fernstone is a native Minecraft client for Alpha and Beta
  multiplayer, up to b1.7.3.** Version 1 joins servers from Alpha 1.0.15 to
  Beta 1.7.3. Classic is not in version 1.
- **The goal: three devices join one b1.7.3 world at the same time.** The
  devices are an iPhone 4S with iOS 6.1.3 (jailbroken), a Mac (arm64), and
  an iPhone 15 Pro Max with iOS 27 in LiveContainer. The server is Project
  Poseidon in offline mode.
- **The client is native code, not Java.** No JVM runs well on the 4S, and
  LiveContainer has no JIT.
- **Offline mode only.** There is no Microsoft login.

## The architecture

- **One repository, one shared core, three thin shells.**

  | Directory | What it holds |
  | --------- | ------------- |
  | `core/world/` | The world model: block states, chunk sections, entities. The renderer reads only this. |
  | `core/proto/beta/` | The protocol adapter for Alpha 1.0.15 to b1.7.3, the version table, and the chunk decoder. |
  | `core/render/` | The GLES2 renderer, the chunk mesher, the 2D drawing, and the models. |
  | `core/assets/` | The zip reader and the PNG decoder for the jar of the user. |
  | `core/` | The client, the physics, the dig times, the network, the menus, the manifests, and the benchmark. |
  | `platform/ios6/` | Objective-C, UIKit, EAGLContext, touches. armv7. |
  | `platform/ios/` | Objective-C, UIKit, EGL on ANGLE (Metal). arm64. An unsigned .ipa. |
  | `platform/macos/` | Objective-C, AppKit, EGL on ANGLE (Metal), keyboard and mouse. arm64. |
  | `platform/cli/` | A client with no window, for the tests on the Mac. |

- **Each shell does three jobs only.** It makes a window and a GL context,
  it reads the input, and it runs the frame loop. Each shell also has a
  file picker for the jar, and a debug command file for the tests. The
  menus, the game, and the drawing are in the core (`core/app.h`).
- **The core is portable C99.** Reason: iOS 6 has an old libc++. C99 builds
  with the old SDK and with the new Xcode.
- **One GLES2 renderer runs on all targets.** iOS 6 uses the system OpenGL
  ES. iOS and macOS use ANGLE on Metal, not the system OpenGL ES. Reason:
  Apple stopped the work on OpenGL ES, and Metal is the path that stays.
- **ANGLE is MetalANGLE, built as a static library.** `scripts/angle.sh`
  fetches commit `ec925142` and joins its libraries into one `libangle.a`
  for each platform. Reason: LiveContainer needs no framework when the
  library is static, and MetalANGLE has Xcode projects for iOS and macOS.
  The shells use EGL with a CAMetalLayer, so they need no MGLKit.
- **The macOS build links the OpenGL framework.** The macOS library of
  MetalANGLE also holds a CGL backend. The app does not use it, but the
  linker needs its symbols.

## The 4S

- **The 4S build is the build of the other 4S apps: clang, the iOS 9.3
  SDK of Theos, and `fix-sdk.sh`, with no Theos makefiles.** The owner
  chose this on 2026-10-24. Reason: the build is proven on this phone. All
  code is ARM mode (`-marm`), because the new Apple linker makes Thumb code
  that crashes on iOS 6 before `main` starts.
- **The scripts reach the 4S with `ssh iphone4s`.** The host is a USB
  tunnel of `iproxy` on port 2222, as for the other 4S apps.
- **The budget of the 4S:** a render distance of 4 chunks, one 256 x 256
  terrain atlas, one VBO for each chunk section, and the meshing on the
  second core. Measured on 2026-10-24: 16.4 ms for each frame at render
  distance 4, in the world of the test server.
- **The data directory of the 4S is `/var/mobile/Media/Fernstone/`.** An
  AFC tool can reach it, so the user can copy the jar there.

## LiveContainer

- **The iOS app has no app extension, no App Group, no push, no special
  entitlement, no fixed bundle ID, and no keychain.**
- **It gets its paths at runtime** with `NSSearchPathForDirectoriesInDomains`.
- **It uses the scene lifecycle.** iOS 27 stops an app without it at
  launch.
- **The user connects to the server by IP address.** The text of the Local
  Network prompt comes from `NSLocalNetworkUsageDescription` in the
  Info.plist of the host app. If a connection fails with no prompt, look at
  this first. If LiveContainer fails, install the same .ipa with SideStore.
- **The user imports the jar with the file picker of iOS.** The app copies
  it to `Documents/jars/<version>.jar`.

## The assets

- **The repository and the packages hold no file of Mojang.** The app
  reads `terrain.png`, the font, the GUI, the items, the icons, and the skin
  from the client jar of the user, for the version that the user selects.
- **Without a jar, the menus use a built-in 5 x 7 pixel font.** The font
  is drawn for Fernstone (`core/render/draw2d.c`).
- **The logo is a mossy cobblestone block in pixel art, drawn for
  Fernstone.** The owner chose it on 2026-10-24. `tools/brand.m` draws it
  from a map of 16 x 16 cells. It is not the texture of Mojang.

## The versions

- **`core/proto/beta/` is the only adapter in version 1.** A table holds
  the differences of each version (`core/proto/beta/version.c`). Each entry
  has the protocol number that the login sends, and the packet layout (2 to
  14). [docs/protocol.md](protocol.md) gives the differences.
- **The block ID tables map the wire IDs of each version to internal
  block states.** The internal IDs are not wire IDs, so a later adapter can
  add its own table and its own blocks.
- **Each version has a manifest with "min" and "recommended" needs**, in
  `manifests/`. `scripts/manifests.sh` puts them into the app.
- **The version browser measures the device on the first launch:** the
  memory (`NSProcessInfo.physicalMemory`), the GL version, and a 2 s chunk
  meshing benchmark. It uses the measured numbers, not a table of device
  names. It tags the newest version that meets "recommended" as
  Recommended, a version that meets only "min" as May lag, and grays out
  the others.
- **The memory is rounded to the nearest 64 MB.** iOS gives a value a
  little under the true size. The 4S gives 505 MB for its 512 MB.
- **The benchmark score is the count of section meshes in one second,
  divided by 10.** Measured on 2026-10-24: the 4S scores 36 and the Mac
  (M-series) scores 1874. The manifests ask for 10. Thus b1.7.3 is
  Recommended on each device.
- **The protocol numbers come from the table of wiki.vg.** The table has
  merged cells for the early Alpha versions, so a1.0.15, a1.0.16, and
  a1.0.17_04 are not sure. These versions use the oldest known layout (2),
  and the version browser marks them as not tested.

## The servers for the tests

- **Project Poseidon runs on the Mac** (`scripts/server.sh`). Beta has no
  LAN mode, because "Open to LAN" came in 1.3. The owner chose this on
  2026-10-24.
- **Uberbukkit runs on the Mac for the older versions** (`scripts/server.sh
  --uber`, port 25566). It accepts the protocols 2 to 14.
- **The spawn protection is off.** Poseidon keeps it in `bukkit.yml`.

## The releases

- **Each milestone is a minor release, from 0.1 to 0.6.** 1.0.0 is the
  goal of version 1: the three devices in one world.
- **A release goes to GitHub, to the Cydia repository, and to the
  SideStore source** (`scripts/release.sh`). The owner chose this on
  2026-10-24.

## Later (not in version 1)

- Adapters for the early releases (1.0 to 1.6.4) and for netty (1.7 to
  1.12.2). 1.3 adds encryption, 1.8 adds compression, and 1.9 changes the
  chunk format. The world model, the block ID tables, and the manifests
  do not block these adapters. ViaProxy with ViaLegacy can help to test one
  adapter against many server versions.
