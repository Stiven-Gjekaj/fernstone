# Changelog

## 1.1.0 (not released)

The menus and the touch controls look like those of Pocket Edition.

- The title screen: the name in cobblestone letters, a turned splash text,
  a strip of moving terrain, and the text in the middle. The join and the
  options screens have their own panels.
- The touch controls: a D-pad with the sneak toggle in its middle, a jump
  button, and the pause and chat buttons at the top right. The GUI pixels
  are bigger on a touch screen.
- A finger aims: a hold breaks the block under the finger, and a tap
  places on it. Cracks show while a block breaks.
- The mobs have the box models and the textures of Beta. A sheep shows
  its wool in its color.
- A dropped item that is not a block shows as a flat sprite.
- The water and the lava move, with the texture simulation of Beta at 20
  ticks a second.
- The inventory screen (E on the Mac, the "..." button on a phone) and the
  chest, workbench, furnace, and dispenser screens. A click moves the
  items with the rules of Beta, and the server confirms each click.
- The sky light and the block light spread again after each block change.
  A torch lights the blocks around it, and a new roof makes a shadow.
- The debug screen shows the light, the block, and the water state at the
  feet of the player.
- A swimmer climbs out of the water over an edge one block high, as in
  Beta. Before, a player in a pond one block deep could not get out.
- The delete key removes the characters of a text field on iOS 6.
  Checked in the iOS 27 simulator. Not checked on the 4S yet.

## 1.0.0 (2026-10-24)

Version 1 joins Alpha and Beta servers up to b1.7.3 from the iPhone 4S on
iOS 6, from the iPhone on iOS 15 or later, and from the Mac.

- The iPhone 4S, the Mac, and the iOS 27 simulator joined one Poseidon
  world at the same time and saw each other. Two clients saw the block
  changes of each other.
- The iPhone 15 Pro Max in LiveContainer is not tested yet. The same .ipa
  ran in the iOS 27 simulator.
- A death screen with a respawn button.
- A block that a change opens gets the light of its neighbors, so a new
  hole is not black.
- A player that stands in the camera is not drawn.

## 0.6.0 (2026-10-24)

The version browser shows the Alpha and Beta versions from a1.0.15 to
b1.7.3, and b1.7.3 is Recommended on each device.

- The version table has 16 versions, one for each packet layout. The
  protocol adapter reads and writes the old layouts: the Alpha login with a
  password, the Alpha inventory, the Alpha place and hotbar packets, and
  the old dig statuses.
- The headless client joined Uberbukkit 2.0.2 with 12 of the versions,
  from a1.1.2_01 to b1.7.3. b1.1_02 and the Alpha versions before a1.1.2_01
  are not tested.
- Each version has a manifest in `manifests/` with "min" and "recommended"
  needs.
- The first launch measures the memory, the GL version, and a 2 s chunk
  meshing benchmark. The 4S scores 36. The Mac scores 1874.
- The logo, the banner, and the icons: a mossy cobblestone block in pixel
  art.

## 0.5.0 (2026-10-24)

The three shells play the game. A player walks, breaks and places blocks,
and chats, and sees the other players and their block changes.

- The physics of Beta: the walk speed, the jump, the step up, the water,
  and the ladders.
- The dig time of each block and tool, as in b1.7.3.
- The touch controls on iOS 6 and iOS: a stick on the left, a look area on
  the right, hold to dig, tap to place, and buttons for jump, sneak, chat,
  and the menu.
- The keyboard and the mouse on the Mac.
- The menus: the server address, the name, the version, the render
  distance, and the jar import.
- The 4S, the Mac, and the iOS 27 simulator joined one Poseidon world at
  the same time and saw each other.

## 0.4.0 (2026-10-24)

The renderer draws the chunks with the terrain atlas from the jar of the
user.

- One VBO for each chunk section, and the meshing on the second core.
- The 4S draws a frame in 16.4 ms at a render distance of 4.
- The text with the font of the jar, the hotbar, the hearts, and the chat.
- The models of the players with their skin.

## 0.3.0 (2026-10-24)

The core speaks the b1.7.3 protocol.

- The handshake, the login, the chunks (0x32, 0x33), the block changes,
  the entities, the chat, and the keep-alive answer.
- `fernstone-cli` joins Poseidon with no window, on the Mac.

## 0.2.0 (2026-10-24)

The iOS app draws a triangle with ANGLE on Metal, and sends the b1.7.3
handshake to Poseidon. The test ran in the iOS 27 simulator.

## 0.1.0 (2026-10-24)

The iOS 6 app draws a GLES2 triangle on the iPhone 4S.
