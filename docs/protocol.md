# The protocols from Alpha 1.0.15 to Beta 1.7.3

This file gives the wire differences that `core/proto/beta/` uses. The
reference is the b1.7.3 protocol (number 14): plain TCP, big-endian fields,
zlib chunk data, and no encryption. The differences come from the source of
Project Poseidon Uberbukkit (`NetServerHandler`, the `Packet*` classes, and
`uk.betacraft.uberbukkit.protocol`). The protocol numbers come from the table
of wiki.vg.

## The versions

The table has the last version of each packet layout. "Layout" is the
protocol of Uberbukkit with the same fields.

| Version | Number in the login | Layout | Strings | Tested |
| ------- | ------------------- | ------ | ------- | ------ |
| b1.7.3 | 14 | 14 | UTF-16 | Poseidon and Uberbukkit |
| b1.6.6 | 13 | 13 | UTF-16 | Uberbukkit |
| b1.5_01 | 11 | 11 | UTF-16 | Uberbukkit |
| b1.4_01 | 10 | 10 | modified UTF-8 | Uberbukkit |
| b1.3_01 | 9 | 9 | modified UTF-8 | Uberbukkit |
| b1.2_02 | 8 | 8 | modified UTF-8 | Uberbukkit |
| b1.1_02 | 8 | 7 | modified UTF-8 | no |
| b1.1_01 | 7 | 7 | modified UTF-8 | Uberbukkit |
| a1.2.6 | 6 | 6 | modified UTF-8 | Uberbukkit |
| a1.2.3_04 | 5 | 5 | modified UTF-8 | Uberbukkit |
| a1.2.2 | 4 | 4 | modified UTF-8 | Uberbukkit |
| a1.2.1_01 | 3 | 3 | modified UTF-8 | Uberbukkit |
| a1.1.2_01 | 2 | 2 | modified UTF-8 | Uberbukkit |
| a1.0.17_04 | 1 | 2 | modified UTF-8 | no |
| a1.0.16 | 13 | 2 | modified UTF-8 | no |
| a1.0.15 | 13 | 2 | modified UTF-8 | no |

"Tested" means that the headless client logged in, received the chunks,
and sent a chat line.

- b1.1_02 sends 8 but uses layout 7. Uberbukkit reads 8 as b1.2 unless its
  own target is 7, so the test on Uberbukkit fails. A real b1.1_02 server
  speaks layout 7.
- Uberbukkit has no layout for the versions before a1.1.2_01. Fernstone uses
  layout 2 for them. No test confirms it.

## The order of the login

1. Send the handshake (0x02) alone, in one write.
2. Wait for the answer of the server. The server reads all the bytes that
   came in as the name, so a login in the same write is lost.
3. Send the login (0x01).
4. Send a packet at least each few seconds. Poseidon sends no keep-alive,
   but it disconnects a client that is silent for 30 s. Fernstone sends 0x0A
   each tick and answers each 0x00.

A kick (0xFF) before the login has modified UTF-8 in each version, because
the server does not know the protocol yet.

## The fields that change

| Packet | Layout | Fields |
| ------ | ------ | ------ |
| 0x01 login, client | 2 | int number, string name, string password |
| | 3 to 10 | int number, string name, string password, long seed, byte dimension |
| | 11 to 14 | int number, string name, long seed, byte dimension |
| 0x01 login, server | 2 | int entity, string, string |
| | 3 to 10 | int entity, string, string, long seed, byte dimension |
| | 11 to 14 | int entity, string, long seed, byte dimension |
| 0x05 | 2 to 6 | int type (-1 main, -2 crafting, -3 armor), short count, items |
| | 7 | int entity, short slot, short item |
| | 8 to 14 | int entity, short slot, short item, short damage |
| 0x07 use entity | 2 to 4 | int user, int target |
| | 5 to 14 | int user, int target, byte left click |
| 0x08 health | 5 and 6 | byte |
| | 7 to 14 | short |
| 0x09 respawn | 5 to 11 | no fields |
| | 12 to 14 | byte dimension |
| 0x0E dig | 2 to 8 | status 0 start, 1 while the client digs, 3 the block broke |
| | 9 to 14 | status 0 start, 2 finish |
| 0x0F place | 2 to 6 | short item, int x, byte y, int z, byte face |
| | 7 | int x, byte y, int z, byte face, item with a byte damage |
| | 8 to 14 | int x, byte y, int z, byte face, item with a short damage |
| 0x10 hold | 2 to 6 | int entity, short item (the server selects the slot with the item) |
| | 7 to 14 | short slot |
| 0x15 pickup | 2 to 7 | no damage field |
| 0x17 object | 2 to 12 | no thrower field |
| | 13 and 14 | int thrower, and three shorts when the thrower is more than 0 |
| 0x18 mob | 2 to 7 | no metadata |
| 0x67 set slot | 7 | item with a byte damage |

All strings are modified UTF-8 before layout 11, and UTF-16 from layout
11. The title of 0x64 is modified UTF-8 in each layout.

## The blocks of each layout

The layouts before 14 do not have some blocks. Fernstone shows a wire ID
that a version does not have as an unknown block.

| Layout | No block |
| ------ | -------- |
| 13 and lower | 29, 33, 34, 36 (the pistons) |
| 11 and lower | 31, 32, 96 (tall grass, dead bush, trapdoor) |
| 10 and lower | 27, 28, 30 (the rails with power, the web) |
| 9 and lower | 95 (the locked chest) |
| 8 and lower | 26, 93, 94 (the bed, the repeaters) |
| 7 and lower | 21 to 25 (lapis, dispenser, sandstone, note block) |
| 2 | 86 to 91 (pumpkin, netherrack, soul sand, glowstone, portal) |

## Rules for each layout

- The stance (the third double of 0x0B and 0x0D from the client) minus the
  y value must be from 0.1 to 1.65, or the server kicks the client.
- From the server, 0x0D sends x, then the stance, then y, then z.
- Poseidon digs five times slower when the head of the player is in water,
  or when the player is not on the ground.
