# Kingdom of Xorx - a simple action/adventure game like ZZT or Kroz
## Overview
This is a very small action/adventure game, where you explore a "large" world, encounter foes and puzzles.

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
- spawners like in Gauntlet
- boulders which can be moved sokoban style
- chop trees with axes
- keys / doors
- treasure chests
- loading / saving the game on statues/campfires
- activator tiles, which can be activated by the player and transmit a signal to adjacent cells (like redstone in Minecraft)
- some kind of asset archive (not just loading the files directly from some directory)
- using SDL3 storage system for cross-plattform compatible directories to store savegames
- some kind of intro screen
- replays
- builtin editor?
