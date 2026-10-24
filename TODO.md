# Roadmap

Each release adds one milestone. The risky work comes first: the
toolchains of the two phones, then the protocol, then the renderer. The
reason for each choice is in [docs/decisions.md](docs/decisions.md). Each
release is a milestone on GitHub, and each item below is an issue in that
milestone.

## 0.1: the toolchain of the 4S (done, 2026-10-24)

- Build an armv7 app for iOS 6 with the toolchain of the other 4S apps.
- Draw one GLES2 triangle on the iPhone 4S.

## 0.2: the toolchain of the iPhone 15 Pro Max (done, 2026-10-24)

- Build MetalANGLE as a static library for iOS and macOS.
- Draw a triangle with EGL on ANGLE in an iOS app with the scene
  lifecycle.
- Send the b1.7.3 handshake (0x02) to Poseidon. The server log shows the
  connection.

## 0.3: the protocol (done, 2026-10-24)

- Connect to Poseidon, send the handshake and the login, receive the
  chunks, and answer the keep-alive.
- Test it with a headless client on the Mac.

## 0.4: the renderer (done, 2026-10-24)

- Draw the chunks with the terrain atlas of the jar.
- Mesh the chunks on the second core, with one VBO for each section.

## 0.5: the game and the three shells (done, 2026-10-24)

- Movement, digging, placing, and chat.
- The shells of iOS 6, iOS, and macOS.

## 0.6: the version browser (done, 2026-10-24)

- The manifests and the device benchmark.
- The older Alpha and Beta versions in the protocol adapter.

## 1.0: three devices in one world (released, 2026-10-24, one test open)

- Install the .ipa on the iPhone 15 Pro Max with SideStore and run it in
  LiveContainer on iOS 27. Join the Poseidon world with the 4S and the Mac
  at the same time. (Not done: this needs the phone in the hands of the
  owner. The same build ran in the iOS 27 simulator.)
- Check that each player sees the others move and sees their block
  changes on the three devices.
- Test b1.1_02 on a real b1.1_02 server, and the Alpha versions before
  a1.1.2_01 on real servers.

## Known problems

- A mob is a colored box. A dropped item that is not a block is not drawn.
- The water and the lava do not move. The texture of the jar is a still
  frame.
- There is no inventory screen. The hotbar shows the items that the server
  gives.
- The light does not change when a block changes. The server sends new
  light only with a new chunk.
