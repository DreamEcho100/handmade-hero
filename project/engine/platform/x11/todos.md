# TODOs

## Urgent TODOs

- [ ] [Raylib] Mirror the X11 platform changes to the Raylib platform.
- [ ] Implement proper detection window focus/active/inactive/minimized, by making the platform detect it and the game is what handles it.
- [ ] Using `#if !defined(DE100_...)` to be able make the integrated game with engine to pass some variables/states from the engine to the game (like memory size, controllers count, etc) while having sensible defaults for when the game is built standalone, this will allow for not needing to have a `GAME_STARTUP` loaded dll since the game will be able to know these values at compile time. or having a dll for configs?
- [ ] Implement proper error handling and reporting for all functions. and refactor what's needed to be on internal/dev mode only or/and every range of time.

## Future TODOs

- [ ] the keyboard input handling currently is tied to the engine which make it hard to use the engine with other projects, not to mention how each backend handle the keyboard input differently _(pull vs event based, keys codes/enums, special keys, keyboard layouts, etc)_.
  - So the following are proposed:
    1. Make the platform handle the keyboard input events and store them in a way that the game can query them.
    - **Cons:**
      - Having to handle different keyboard layouts, key codes, special keys, etc per game/platform.
      - Handling pull vs event based input.
      - Adding more complexity to the platform code.
      - Could be less efficient if the game needs to query the keyboard state frequently.
    - **Pros:**
      - Will provide a unified way for the game to handle keyboard input.
      - Will make the code cleaner and easier to read across different games.
    2. Make the game handle the keyboard input directly by providing a way for the platform to send the keyboard input events to the game. So the game will be responsible for handling the keyboard input. and it will also choose the platform to use with and it's perimitives of handling _(for example when following handmade here we can handle both the X11 and Raylib for learning, but on other games we can choose to use the Raylib only for simplicity, features, and cross platform support)_. for example the `engine` dir is separate from the `game` itself _(while being dynamically loaded on dev and could be built directly as one unit on prod by having different build scripts and using macros/preprocessors)_, but the `game` will have an `adapter` folder that will have the platform specific code to handle the keyboard input and send it to the game logic. and it's linked correctly on the build script and the game code will use the adapter to handle the keyboard input.
    - **Cons:**
      - It will depend on the platform selected for every game on how hard/annoying it is.
      - Could lead to code duplication if the game needs to handle multiple platforms.
    - **Pros:**
      - Will provide more flexibility for the game to handle keyboard input.
      - Will make the platform code simpler and easier to maintain. _(as in not having to handle keyboard input at all and leave it to the platform)_
    3. Using a 3rd party keyboard input handling library.
    - **Cons:**
      - Will add more complexity to the project.
      - Will add more dependencies to the project.
    - **Pros:**
      - Will provide a way to handle keyboard input without having to worry about the platform specific code.
      - Will make the code cleaner and easier to read across different games.
  - After evaluating the pros and cons of each approach, I think the best approach would be to go with option 2, as it will provide more flexibility for the game to handle keyboard input and will make the platform code simpler and easier to maintain. and maybe looking more into option 3 if needed later on.
- [ ] Look at the `engine/_common/file.c` `SET_ERROR_DETAIL` and `CLEAR_ERROR_DETAIL` to see if they can be used to store the error details in a way that can be used by the game to display the error message.
- [ ] Make sure when windows is detected, for performance to use `WIN32_LEAN_AND_MEAN`, `NOMINMAX`, and any other needed defines.
- [ ] Enable the build script to be done on parellel when needed
- [ ] Improve the `engine/build-common.sh` and `handmadehero/build.sh` scripts to avoid code duplication. and make sure the `build-common.sh` provides all what's needed to be able t integrate the engine with any game project.
- [ ] Support dev mode _(hot reloading, debug logs, etc)_ and prod mode _(no debug logs, no hot reloading, etc)_

## Done TODOs

- [x] [X11] Audio on has several issues:
  - [x] When connecting to a bluetooth headphones and starting the the sound is low and have small ticks from time to time.
  - [x] And if connecting and disconnecting during the game, the audio just stops
  - [x] The audio doesn't work when it's on the background
- [x] Handle different `DYNAMIC LIBRARIES` type based on the platform, currently `so` is only used for linux, but it should be `dylib` for mac and `dll` for windows. no need to handle the `build.sh` script for that for now though.
- [x] Merge the `PlatformAudioConfig` to `PlatformConfig`
- [x] Use the custom `engine/_common/time.h` implementation instead of the `#include <time.h>` on the platforms and game code.
- [x] Use AI/LLM to audit the `engine/_common/*` files for
  - Performance.
  - Best practices.
  - Cross platform compatibility.
  - Not storing redundant data, for example if error messages being stored, it can be replaced with codes for them that I can use a function to translate on .need
  - Be extensive on error messages.
  - Use `#if DE100_INTERNAL && DE100_SLOW` when needed for a better dev experience.
  - Code quality.
  - Style.
- Suggest improvements where needed.
- Never use `goto` statements.
- [x] Move shared logic between the platforms to separate file to avoid unneeded duplication.
- [x] Change `DE100_POSIX` to `DE100_IS_GENERIC_POSIX` and define `DE100_IS_GENERIC_WINDOWS`, and implement their usage where needed.
