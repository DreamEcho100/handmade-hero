# Todos

## All the Platforms

- [ ] Move shared logic between the platforms to separate file to avoid unneeded duplication.
- [ ] Implement proper detection window focus/active/inactive/minimized, by making the platform detect it and the game is what handles it.
- [ ] Using `#if !defined(DE100_...)` to be able make the integrated game with engine to pass some variables/states from the engine to the game (like memory size, controllers count, etc) while having sensible defaults for when the game is built standalone, this will allow for not needing to have a `GAME_STARTUP` loaded dll since the game will be able to know these values at compile time. or having a dll for configs?
- [ ] Merge the `PlatformAudioConfig` to `PlatformConfig`
- [ ] Handle different `DYNAMIC LIBRARIES` type based on the platform, currently `so` is only used for linux, but it should be `dylib` for mac and `dll` for windows. no need to handle the `build.sh` script for that for now though.
- [ ] Use the custom `./engine/_common/time.h` implementation instead of the `#include <time.h>` on the platforms and game code.
- [ ] Improve the `engine/build-common.sh` and `handmadehero/build.sh` scripts to avoid code duplication. and make sure the `build-common.sh` provides all what's needed to be able t integrate the engine with any game project.
- [ ] Support dev mode _(hot reloading, debug logs, etc)_ and prod mode _(no debug logs, no hot reloading, etc)_
- [ ] Implement proper error handling and reporting for all functions. and refactor what's needed to be on internal/dev mode only or/and every range of time.

## X11 Platform

- [ ] Audio on has several issues:
  - [ ] When connecting to a bluetooth headphones and starting the the sound is low and have small ticks from time to time.
  - [ ] And if connecting and disconnecting during the game, the audio just stops
  - [ ] The audio doesn't work when it's on the background

## Raylib Platform

- [ ] Mirror the X11 platform changes to the Raylib platform.
