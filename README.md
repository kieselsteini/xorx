# Kingdom of Xorx - a simple action/adventure game like ZZT, Kingdom of Kroz or Dungeons of Grimlor
## Overview
This is a very small action/adventure game, where you explore a "large" world, encounter foes and puzzles.

## World Format
The whole world is stored in a BMP file called `world.bmp`. The color of every pixel denotes a single cell in the game.
The size of the bitmap is 512 x 256 pixels, which is the excat size of the game world. You see always an area of 32x16 pixels/cells.

### Color Codes
| Color | Description |
| --- | --- |
| #000000 | Free space. |
| #4e4a4e | Solid wall, cannot be destroyed by any means. |
| #70402a | Breakable wall, can be destroyed by arrows. |
| #004000 | Trees, cannot be destroyed by any means. |
| #4a2a1b | Dead trees, cannot be destroyed by any means. |
| #008000 | Grass. Player destroys it on touch. |
| #000096 | Water. Blocks movement, but allows arrows/bolts to fly over. Boulders will destroy water. |
| #8595a1 | Boulder. Can be pushed by player to empty spaces, water or monsters. |
| #ffffff | Player start position. There should be only **ONE** pixel of this kind! |
| #400000 | Monster Level 1. Deals 1 damage. Will be destroyed by arrows / player. |
| #800000 | Monster Level 2. Deals 2 damage. Will be destroyed by player. Arrow will degrade it to level 1. |
| #c00000 | Monster Level 3. Deals 3 damage. Will be destroyed by player. Arrow will degrade it to level 2. |
| #ff0000 | Monster Level 4. Deals 4 damage. Will be destroyed by player. Arrow will degrade it to level 3. |
| #ff8000 | Bolt Trap. Shoots bolts in all 4 directions in a regular interval. |
| #ff6400 | Monster Shrine. Spawns random monster in regular intervals in a random direction. You can make it less dangerous, if you block some sides. |
| #6dc2ca | Teleporter. Player will be teleported to corresponding teleporter. |

### Teleporters
Teleporters are a bit tricky to get used to. If a player touches a teleporter, it will scan in the direction the player moved until it finds another teleporter. Then will place the player one cell adhead in that direction. Think of a pipe in a straight line. You enter the pipe on one end, and leave it at the other end.

## Building

### Dependencies
You need a somewhat modern (C11) C compiler and [SDL3](https://libsdl.org/) libraries in order to build this game.
It is known to work with Clang 17 and GCC 15.

### Unix/Linux/macOS
On those systems it's fairly simple to build this game. Just type ```make``` and you are ready.
```sh
make
```

### Windows
Not yet :)

### Web/WASM
Not yet :)

## Design Goals
- keep everything in one C file
- keep it as simple as possible
- tile based movement
- real-time gameplay
- cellular automata structure of the world

## Planned features
- magic flasks to wipe out any enemy in sight
- chop trees with axes
- keys / doors
- treasure chests
- loading / saving the game on statues/campfires
- activator tiles, which can be activated by the player and transmit a signal to adjacent cells (like redstone in Minecraft)
- some kind of asset archive (not just loading the files directly from some directory)
- using SDL3 storage system for cross-plattform compatible directories to store savegames
- some kind of intro screen
- replays
