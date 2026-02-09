# INPUT SYSTEM REFERENCE GUIDE

> The user is asking for a comprehensive summary of the input system approaches we discussed, along with research resources and potential challenges. Let me organize this thoroughly.
>
> They want:
>
> A summary of the approaches as a nested list of steps
> Search terms/queries for further research
> Resources (books, articles, tutorials)
> Potential challenges/pitfalls and solutions
> Let me be thorough and practical here.

## Input System Architecture Reference Guide

## 1. Acknowledgment of Your Goals

You're absolutely right to be thinking ahead. Building an engine that can:

- Support multiple backends (X11, Raylib, SDL, OpenGL, Vulkan)
- Handle diverse input methods (keyboard, mouse, gamepad, touch)
- Support complex input (combos, gestures, recording/playback)
- Allow remappable controls
- Be reusable across multiple games

...is a legitimate architectural goal. The key is to build toward it incrementally rather than trying to design the perfect system upfront.

### 2. Summary of Approaches (Nested Steps)

#### Approach A: Current (Direct Mapping)

```
Status: What you have now
Effort: Already done
Best for: Single game, learning phase
```

```
1. Backend Layer
   └── Directly maps platform input → game actions
       ├── X11 KeySym → controller->move_up
       ├── Linux joystick → controller->stick_avg_x
       └── Raylib key → controller->action_down

2. Game Layer
   └── Reads GameControllerInput directly
       └── if (controller->move_up.ended_down) { ... }

Problems:
├── Backend knows about game-specific actions
├── Adding new backend requires duplicating all mappings
└── No runtime remapping possible
```

```
#### Approach B: Simple Physical Layer (Option C - Recommended First Step)
Status: Recommended next step
Effort: ~6 hours
Best for: Multi-backend support, clean separation
```

```
1. Create Physical Input Layer
   ├── 1.1 Create engine/input/physical_input.h
   │   ├── Define PhysicalKey enum (PKEY_A, PKEY_W, PKEY_SPACE, etc.)
   │   ├── Define PhysicalButton enum (PBTN_A, PBTN_DPAD_UP, etc.)
   │   ├── Define PhysicalAxis enum (PAXIS_LEFT_STICK_X, etc.)
   │   ├── Define PhysicalInputState struct
   │   │   ├── bool keys[PKEY_COUNT]
   │   │   ├── bool keys_prev[PKEY_COUNT]
   │   │   └── PhysicalGamepadState gamepads[MAX_GAMEPADS]
   │   └── Define inline helpers
   │       ├── pkey_pressed(input, key)
   │       ├── pkey_just_pressed(input, key)
   │       ├── pbtn_pressed(input, gamepad, button)
   │       └── paxis_value(input, gamepad, axis)
   │
   └── 1.2 Create engine/input/physical_input.c (optional)
       └── Any non-inline implementation if needed

2. Modify Backend Layer (per backend)
   ├── 2.1 X11 Keyboard (inputs/keyboard.c)
   │   ├── Create x11_keysym_to_physical() translation function
   │   ├── Modify x11_handle_key_press() to set input->keys[pkey]
   │   └── Remove all game action knowledge
   │
   ├── 2.2 X11 Joystick (inputs/joystick.c)
   │   ├── Create linux_button_to_physical() translation
   │   ├── Create linux_axis_to_physical() translation
   │   └── Modify linux_poll_joystick() to fill PhysicalInputState
   │
   ├── 2.3 X11 Backend Main (backend.c)
   │   ├── Replace GameInput with PhysicalInputState
   │   ├── Call physical_input_begin_frame() each frame
   │   └── Pass PhysicalInputState* to game
   │
   └── 2.4 Raylib Backend (similar changes)
       └── Map Raylib keys/buttons to PhysicalKey/PhysicalButton

3. Modify Game Layer
   ├── 3.1 Update game function signature
   │   └── Change from GameInput* to void* raw_input
   │
   ├── 3.2 Create game-specific mapping functions (in game.c)
   │   ├── static inline bool input_move_up(PhysicalInputState* in)
   │   ├── static inline bool input_move_down(PhysicalInputState* in)
   │   ├── static inline bool input_attack(PhysicalInputState* in)
   │   ├── static inline real32 input_move_x(PhysicalInputState* in)
   │   └── ... (all game actions as inline functions)
   │
   └── 3.3 Use mapping functions in game update
       └── if (input_move_up(input)) { player.y += speed; }

Benefits achieved:
├── Backends are game-agnostic (reusable for any game)
├── All input mapping visible in one place (game.c)
├── Zero performance overhead (inline functions)
└── Foundation for more advanced features
```

#### Approach C: Full Binding System (Future Enhancement)

```
Status: Implement when you need runtime remapping
Effort: ~14-20 hours
Best for: User-configurable controls, multiple games
```

```
1. Extend Physical Layer (if not done)
   └── Same as Approach B, Step 1

2. Create Action Definition System
   ├── 2.1 Create engine/input/action_types.h
   │   ├── Define InputSourceType enum
   │   │   ├── INPUT_SOURCE_KEY
   │   │   ├── INPUT_SOURCE_BUTTON
   │   │   ├── INPUT_SOURCE_AXIS
   │   │   └── INPUT_SOURCE_AXIS_AS_BUTTON
   │   │
   │   ├── Define InputSource struct
   │   │   ├── InputSourceType type
   │   │   └── union { PhysicalKey key; PhysicalButton btn; ... }
   │   │
   │   ├── Define ActionBinding struct
   │   │   ├── int action_id
   │   │   ├── InputSource sources[MAX_BINDINGS_PER_ACTION]
   │   │   └── int source_count
   │   │
   │   └── Define ActionMap struct
   │       ├── ActionBinding* bindings
   │       ├── int binding_count
   │       └── real32 axis_threshold
   │
   └── 2.2 Create engine/input/action_system.c
       ├── action_map_create()
       ├── action_map_destroy()
       ├── action_bind()
       ├── action_unbind()
       ├── action_is_pressed()
       ├── action_just_pressed()
       ├── action_get_value()
       └── action_map_load_from_file() / save_to_file()

3. Create Per-Game Action Definitions
   ├── 3.1 Create game/input_actions.h
   │   └── Define game-specific action enum
   │       ├── ACTION_MOVE_UP
   │       ├── ACTION_MOVE_DOWN
   │       ├── ACTION_ATTACK
   │       ├── ACTION_PAUSE
   │       └── ACTION_COUNT
   │
   └── 3.2 Create game/input_config.c
       ├── Create default bindings
       │   ├── action_bind(map, ACTION_MOVE_UP, PKEY_W)
       │   ├── action_bind(map, ACTION_MOVE_UP, PBTN_DPAD_UP)
       │   └── action_bind(map, ACTION_MOVE_UP, PAXIS_LEFT_Y, -0.5)
       │
       └── Load user config if exists

4. Update Game to Use Action System
   └── In game_update_and_render():
       ├── if (action_is_pressed(map, ACTION_MOVE_UP)) { ... }
       ├── real32 move_x = action_get_value(map, ACTION_MOVE_X)
       └── if (action_just_pressed(map, ACTION_ATTACK)) { ... }

5. Add Configuration UI (Optional)
   ├── 5.1 Create rebinding menu
   │   ├── Display current bindings
   │   ├── "Press key to rebind" prompt
   │   └── Conflict detection
   │
   └── 5.2 Save/Load config
       ├── action_map_save_to_file("controls.cfg")
       └── action_map_load_from_file("controls.cfg")

Benefits achieved:
├── Runtime key remapping
├── User-configurable controls
├── Easy to add new actions
├── Binding data separate from code
└── Serializable configuration
```

#### Approach D: Advanced Input System (Long-term Goal)

```
Status: Future architecture goal
Effort: ~40-80 hours (substantial project)
Best for: Full game engine with complex input needs
```

```
1. Physical Layer (from Approach B)
   └── Already complete

2. Binding Layer (from Approach C)
   └── Already complete

3. Input Recording/Playback System
   ├── 3.1 Create engine/input/input_recorder.h
   │   ├── Define InputFrame struct
   │   │   ├── uint64_t frame_number
   │   │   ├── PhysicalInputState state
   │   │   └── real64 timestamp
   │   │
   │   ├── Define InputRecording struct
   │   │   ├── InputFrame* frames
   │   │   ├── int frame_count
   │   │   └── int capacity
   │   │
   │   └── Define recorder/player functions
   │       ├── recording_start()
   │       ├── recording_stop()
   │       ├── recording_save()
   │       ├── recording_load()
   │       ├── playback_start()
   │       └── playback_get_frame()
   │
   └── 3.2 Integrate with game loop
       ├── If recording: save input each frame
       └── If playing: read input from recording instead of hardware

4. Combo/Gesture Recognition System
   ├── 4.1 Create engine/input/input_sequence.h
   │   ├── Define InputEvent struct
   │   │   ├── InputSource source
   │   │   ├── EventType type (PRESS, RELEASE, HOLD)
   │   │   └── real32 timestamp
   │   │
   │   ├── Define InputPattern struct
   │   │   ├── InputEvent* events
   │   │   ├── int event_count
   │   │   ├── real32 max_duration
   │   │   └── real32 tolerance_ms
   │   │
   │   └── Define ComboMatcher struct
   │       ├── InputPattern patterns[MAX_PATTERNS]
   │       ├── int pattern_count
   │       └── InputEvent history[HISTORY_SIZE]
   │
   ├── 4.2 Create engine/input/input_sequence.c
   │   ├── combo_define(name, pattern_string)
   │   │   └── "↓ ↘ → + P" → parse to InputPattern
   │   ├── combo_check(matcher, history)
   │   └── combo_reset(matcher)
   │
   └── 4.3 Common patterns
       ├── Fighting game: quarter-circle, charge, etc.
       ├── Rhythm game: timed sequences
       └── Gesture: swipe, pinch, etc.

5. Touch/Gesture Layer (for mobile/touch)
   ├── 5.1 Create engine/input/touch_input.h
   │   ├── Define TouchPoint struct
   │   │   ├── int id
   │   │   ├── real32 x, y
   │   │   ├── real32 pressure
   │   │   └── TouchPhase phase (BEGAN, MOVED, ENDED)
   │   │
   │   ├── Define TouchState struct
   │   │   ├── TouchPoint touches[MAX_TOUCHES]
   │   │   └── int touch_count
   │   │
   │   └── Define GestureRecognizer
   │       ├── gesture_tap_check()
   │       ├── gesture_swipe_check()
   │       ├── gesture_pinch_check()
   │       └── gesture_rotate_check()
   │
   └── 5.2 Virtual joystick for touch
       ├── Create on-screen joystick zones
       └── Convert touch position → axis values

6. Input Debugging/Visualization
   ├── 6.1 Create debug overlay
   │   ├── Show current input state
   │   ├── Show action bindings
   │   ├── Show combo detection state
   │   └── Show input history timeline
   │
   └── 6.2 Create input testing mode
       ├── Display all pressed keys/buttons
       └── Show axis values in real-time

7. Multi-player Input Handling
   ├── 7.1 Create engine/input/input_routing.h
   │   ├── Define InputDevice struct
   │   │   ├── DeviceType type
   │   │   ├── int device_id
   │   │   └── int player_id (or -1 if unassigned)
   │   │
   │   └── Define InputRouter struct
   │       ├── InputDevice devices[MAX_DEVICES]
   │       ├── int device_count
   │       └── ActionMap* player_maps[MAX_PLAYERS]
   │
   └── 7.2 Device assignment
       ├── "Press START to join"
       ├── Track which device → which player
       └── Support hot-plug

Benefits achieved:
├── Complete input abstraction
├── Recording for replay/debugging
├── Complex combo detection
├── Touch/mobile support
├── Multi-player ready
└── Full debugging tools
```

## 3. Search Terms and Queries

### Core Concepts

```
Input System Architecture:
├── "game input abstraction layer"
├── "platform independent input system"
├── "input action mapping game engine"
├── "game engine input binding system"
├── "physical key vs logical action game"
└── "decoupling input from game logic"

Data-Oriented Input:
├── "data oriented input handling games"
├── "cache friendly input processing"
├── "input state vs input events"
├── "polling vs event driven input"
└── "input buffering game engine"

Platform-Specific:
├── "X11 keyboard input handling"
├── "Linux joystick API tutorial"
├── "evdev input Linux"
├── "XInput vs DirectInput"
├── "SDL input abstraction"
├── "GLFW input system"
└── "Raylib input handling"
```

### Advanced Features

```
Recording/Playback:
├── "game input recording replay"
├── "deterministic replay system games"
├── "input playback testing games"
├── "TAS tool assisted speedrun input"
└── "demo recording game engine"

Combo Systems:
├── "fighting game input buffer"
├── "combo detection algorithm"
├── "input sequence recognition"
├── "motion input fighting games"
├── "command interpreter fighting game"
└── "input leniency fighting games"

Remapping:
├── "runtime key rebinding game"
├── "control customization game engine"
├── "input configuration save load"
├── "accessibility input remapping"
└── "one handed gaming controls"

Touch/Gesture:
├── "touch input game engine"
├── "virtual joystick mobile games"
├── "gesture recognition game"
├── "multitouch handling games"
└── "swipe detection algorithm"
```

```
Architecture Patterns
├── "command pattern input handling"
├── "input manager singleton pattern"
├── "observer pattern input events"
├── "strategy pattern input mapping"
├── "state machine input handling"
└── "event bus input system"
```

## 4. Resources

### Books

Game Engine Architecture:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ 📚 "Game Engine Architecture" (3rd Edition) - Jason Gregory               │
│    Chapter 8: Human Interface Devices                                      │
│    - Input device abstraction                                              │
│    - Dead zones and filtering                                              │
│    - Action mapping                                                         │
│    - Best resource for engine-level input design                           │
├─────────────────────────────────────────────────────────────────────────────┤
│ 📚 "Game Programming Patterns" - Robert Nystrom                            │
│    Chapter: Command Pattern                                                 │
│    - Input abstraction patterns                                            │
│    - Undo/redo (useful for input recording)                                │
│    FREE: http://gameprogrammingpatterns.com/                               │
├─────────────────────────────────────────────────────────────────────────────┤
│ 📚 "Handmade Hero" (Video Series) - Casey Muratori                         │
│    Days 6-13: Input handling                                               │
│    Days 20-21: Audio timing (similar polling patterns)                     │
│    FREE: https://handmadehero.org/                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│ 📚 "Game Coding Complete" (4th Edition) - Mike McShaffry                   │
│    Chapter 9: Input and Event Management                                   │
│    - Event-driven input                                                    │
│    - Controller support                                                    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Articles and Tutorials

```
Input System Design:
├── GDC Talk: "The Game Controller Dream" - Zach Gage
│   └── https://www.youtube.com/watch?v=wN2iqmFCWSk
│
├── "Designing a Flexible Input System" - Game Developer Magazine
│   └── Search: "gamasutra flexible input system"
│
├── "Input in One Frame" - Raph Koster
│   └── https://www.raphkoster.com/
│
└── "How to make a simple input manager in C++" - TheCherno
    └── https://www.youtube.com/watch?v=SfF_PsK0ZqI

Fighting Game Input:
├── "The Game Design of Fighting Games" - Core-A Gaming
│   └── https://www.youtube.com/c/CoreAGaming
│
├── "Input Priority in Fighting Games"
│   └── Search: "input priority kara cancel"
│
└── Dustloop Wiki (fighting game mechanics)
    └── https://www.dustloop.com/

Platform-Specific:
├── X11 Input:
│   ├── Xlib Programming Manual
│   │   └── https://tronche.com/gui/x/xlib/
│   └── "X11 input handling tutorial"
│
├── Linux Joystick:
│   ├── Linux Joystick API documentation
│   │   └── /usr/include/linux/joystick.h
│   └── evdev documentation
│       └── https://www.freedesktop.org/software/libevdev/doc/latest/
│
└── SDL Input:
    └── SDL Wiki: https://wiki.libsdl.org/CategoryInput
```

### Open Source References

```
Study These Codebases:
┌─────────────────────────────────────────────────────────────────────────────┐
│ 🔧 SDL2 (Simple DirectMedia Layer)                                         │
│    Location: src/events/SDL_keyboard.c, SDL_joystick.c                     │
│    Why: Industry-standard input abstraction                                │
│    Link: https://github.com/libsdl-org/SDL                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🔧 GLFW                                                                     │
│    Location: src/input.c, src/x11_window.c                                 │
│    Why: Simple, clean input handling                                       │
│    Link: https://github.com/glfw/glfw                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🔧 Raylib                                                                   │
│    Location: src/rcore.c (input section)                                   │
│    Why: You're already using it                                            │
│    Link: https://github.com/raysan5/raylib                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🔧 Godot Engine                                                             │
│    Location: core/input/                                                   │
│    Why: Full action mapping system                                         │
│    Link: https://github.com/godotengine/godot                              │
│    Specific: core/input/input_map.cpp                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🔧 Bevy (Rust game engine)                                                  │
│    Location: crates/bevy_input/                                            │
│    Why: Modern, clean architecture                                         │
│    Link: https://github.com/bevyengine/bevy                                │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🔧 GGPO (Rollback netcode library)                                          │
│    Why: Input recording for netplay                                        │
│    Link: https://github.com/pond3r/ggpo                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ 🔧 MAME (Arcade emulator)                                                   │
│    Location: src/emu/input                                                 │
│    Why: Supports insane variety of input devices                           │
│    Link: https://github.com/mamedev/mame                                   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Documentation

```
Platform APIs:
├── X11/Xlib
│   ├── man XNextEvent
│   ├── man XLookupKeysym
│   └── /usr/include/X11/keysymdef.h
│
├── Linux Input
│   ├── /usr/include/linux/input.h
│   ├── /usr/include/linux/joystick.h
│   └── evdev: /usr/include/libevdev-1.0/libevdev/libevdev.h
│
└── ALSA (for timing comparison)
    └── https://www.alsa-project.org/alsa-doc/
```

## 5. Potential Challenges and Pitfalls

### Architecture Challenges

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 1: Over-Engineering Too Early                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Building a complex binding system before you know what features you need.   │
│ You spend weeks on an input system that doesn't match actual game needs.    │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Months of work on input, no game progress                                 │
│ - Input system has features you never use                                   │
│ - Constant refactoring as you discover what you actually need               │
│                                                                             │
│ SOLUTION:                                                                   │
│ - Start with Option B (Simple Physical)                                     │
│ - Add features ONLY when you have a concrete use case                       │
│ - Write down what you wish you had, then implement next iteration           │
│ - "Make it work, make it right, make it fast" - Kent Beck                   │
│                                                                             │
│ CASEY QUOTE:                                                                │
│ "If you don't have the problem yet, you don't know what the solution        │
│  should look like."                                                         │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 2: Leaky Abstractions                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Your "platform-independent" input layer still leaks platform details.       │
│ E.g., keyboard scan codes that only exist on certain keyboards.             │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - PhysicalKey enum has 500+ entries for obscure keys                        │
│ - Different backends handle same key differently                            │
│ - Some keys work on X11 but not on Raylib                                   │
│                                                                             │
│ SOLUTION:                                                                   │
│ - Only abstract keys you ACTUALLY USE                                       │
│ - Have PKEY_UNKNOWN for anything unusual                                    │
│ - Test on all backends regularly (don't let them drift)                     │
│ - Accept that 100% abstraction is impossible                                │
│                                                                             │
│ EXAMPLE FIX:                                                                │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // DON'T try to abstract everything                                  │    │
│ │ typedef enum {                                                       │    │
│ │     PKEY_A, PKEY_B, ..., PKEY_Z,           // Common letters         │    │
│ │     PKEY_0, ..., PKEY_9,                   // Numbers                │    │
│ │     PKEY_UP, PKEY_DOWN, PKEY_LEFT, PKEY_RIGHT,  // Navigation        │    │
│ │     PKEY_SPACE, PKEY_ENTER, PKEY_ESCAPE,   // Common controls        │    │
│ │     // That's it! ~50 keys, not 500                                  │    │
│ │     PKEY_UNKNOWN  // Everything else                                 │    │
│ │ } PhysicalKey;                                                       │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 3: Binding Conflicts (Continued)                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ EXAMPLE CODE:                                                               │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ typedef enum {                                                       │    │
│ │     BIND_RESULT_SUCCESS,                                             │    │
│ │     BIND_RESULT_CONFLICT,       // Another action uses this key      │    │
│ │     BIND_RESULT_PROTECTED,      // Can't unbind essential actions    │    │
│ │     BIND_RESULT_INVALID_KEY     // Key doesn't exist                 │    │
│ │ } BindResult;                                                        │    │
│ │                                                                      │    │
│ │ BindResult action_bind(ActionMap* map, int action, PhysicalKey key) {│    │
│ │     // Check for conflicts                                           │    │
│ │     for (int i = 0; i < map->binding_count; i++) {                   │    │
│ │         if (binding_uses_key(&map->bindings[i], key)) {              │    │
│ │             // Conflict! Return info but allow it                    │    │
│ │             map->last_conflict_action = map->bindings[i].action_id;  │    │
│ │             // Still bind (user can have conflicts if they want)     │    │
│ │         }                                                            │    │
│ │     }                                                                │    │
│ │     // ... do the binding                                            │    │
│ │     return BIND_RESULT_SUCCESS;                                      │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // In UI:                                                            │    │
│ │ if (result == BIND_RESULT_CONFLICT) {                                │    │
│ │     show_warning("This key is also bound to: %s",                    │    │
│ │                  get_action_name(map->last_conflict_action));        │    │
│ │ }                                                                    │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 4: Frame Timing and Input Latency                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Input feels "laggy" or "unresponsive". Button presses are missed.           │
│ Fast button mashing doesn't register all presses.                           │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Player presses button but nothing happens                                 │
│ - Input works in one backend but not another                                │
│ - Fast repeated presses get "eaten"                                         │
│ - Input response varies with frame rate                                     │
│                                                                             │
│ ROOT CAUSES:                                                                │
│ 1. Polling input AFTER game update (1 frame delay)                          │
│ 2. Not tracking "just pressed" vs "is pressed"                              │
│ 3. Event queue overflow (not processing all events)                         │
│ 4. VSync causing input to wait                                              │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // CORRECT frame order:                                              │    │
│ │ while (game_running) {                                               │    │
│ │     // 1. FIRST: Poll input (before any game logic)                  │    │
│ │     physical_input_begin_frame(&input);  // Shift current→previous   │    │
│ │     poll_all_input_devices(&input);      // Get fresh state          │    │
│ │     process_all_pending_events(&input);  // Drain event queue!       │    │
│ │                                                                      │    │
│ │     // 2. THEN: Update game with fresh input                         │    │
│ │     game_update(&input);                                             │    │
│ │                                                                      │    │
│ │     // 3. THEN: Render                                               │    │
│ │     game_render();                                                   │    │
│ │                                                                      │    │
│ │     // 4. FINALLY: Swap buffers (VSync blocks here, not before)      │    │
│ │     swap_buffers();                                                  │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // Track transitions for "just pressed"                              │    │
│ │ bool pkey_just_pressed(PhysicalInputState* in, PhysicalKey key) {    │    │
│ │     return in->keys[key] && !in->keys_prev[key];                     │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // IMPORTANT: Process ALL pending events, not just one!              │    │
│ │ void process_all_pending_events(PhysicalInputState* input) {         │    │
│ │     XEvent event;                                                    │    │
│ │     while (XPending(display) > 0) {  // WHILE, not IF!               │    │
│ │         XNextEvent(display, &event);                                 │    │
│ │         handle_event(&event, input);                                 │    │
│ │     }                                                                │    │
│ │ }                                                                    │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 5: Gamepad Hot-Plug                                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ User plugs in controller mid-game. Or unplugs it. Game crashes or freezes.  │
│ Controller that was player 1 becomes player 2.                              │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Crash when controller disconnected                                        │
│ - Controller stops working until game restart                               │
│ - Controller indices shuffle unexpectedly                                   │
│ - Game hangs trying to read disconnected device                             │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // Check connection every frame (cheap operation)                    │    │
│ │ void poll_gamepads(PhysicalInputState* input) {                      │    │
│ │     for (int i = 0; i < MAX_GAMEPADS; i++) {                         │    │
│ │         int fd = gamepad_fds[i];                                     │    │
│ │                                                                      │    │
│ │         // Check if still connected                                  │    │
│ │         if (fd >= 0) {                                               │    │
│ │             // Try to read - if fails, mark disconnected             │    │
│ │             struct js_event event;                                   │    │
│ │             int result = read(fd, &event, sizeof(event));            │    │
│ │             if (result < 0 && errno != EAGAIN) {                     │    │
│ │                 // Disconnected!                                     │    │
│ │                 close(fd);                                           │    │
│ │                 gamepad_fds[i] = -1;                                 │    │
│ │                 input->gamepads[i].connected = false;                │    │
│ │                 printf("🎮 Gamepad %d disconnected\n", i);           │    │
│ │             }                                                        │    │
│ │         }                                                            │    │
│ │     }                                                                │    │
│ │                                                                      │    │
│ │     // Periodically check for new controllers                        │    │
│ │     static int scan_counter = 0;                                     │    │
│ │     if (++scan_counter > 60) {  // Every ~1 second                   │    │
│ │         scan_counter = 0;                                            │    │
│ │         scan_for_new_gamepads(input);                                │    │
│ │     }                                                                │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // Use STABLE IDs, not array indices                                 │    │
│ │ typedef struct {                                                     │    │
│ │     int device_id;        // Stable ID from OS                       │    │
│ │     int slot_index;       // Our array index (may change!)           │    │
│ │     char name[64];        // "Xbox Controller" etc                   │    │
│ │     bool connected;                                                  │    │
│ │ } GamepadInfo;                                                       │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 6: Dead Zones and Stick Drift                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Analog stick never returns to exactly 0. Character slowly walks when        │
│ player isn't touching the controller. Old controllers have worse drift.     │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Character moves by itself                                                 │
│ - Camera slowly rotates                                                     │
│ - Menu cursor drifts                                                        │
│ - Different controllers have different behavior                             │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // Circular dead zone (better than square)                           │    │
│ │ typedef struct {                                                     │    │
│ │     real32 x;                                                        │    │
│ │     real32 y;                                                        │    │
│ │ } Vec2;                                                              │    │
│ │                                                                      │    │
│ │ Vec2 apply_deadzone_circular(real32 raw_x, real32 raw_y,             │    │
│ │                               real32 deadzone) {                     │    │
│ │     Vec2 result = {0, 0};                                            │    │
│ │                                                                      │    │
│ │     real32 magnitude = sqrtf(raw_x * raw_x + raw_y * raw_y);         │    │
│ │     if (magnitude < deadzone) {                                      │    │
│ │         return result;  // Inside dead zone, return zero             │    │
│ │     }                                                                │    │
│ │                                                                      │    │
│ │     // Normalize and rescale                                         │    │
│ │     real32 normalized_x = raw_x / magnitude;                         │    │
│ │     real32 normalized_y = raw_y / magnitude;                         │    │
│ │                                                                      │    │
│ │     // Rescale to 0-1 range outside dead zone                        │    │
│ │     real32 rescaled_magnitude = (magnitude - deadzone) /             │    │
│ │                                  (1.0f - deadzone);                  │    │
│ │     if (rescaled_magnitude > 1.0f) rescaled_magnitude = 1.0f;        │    │
│ │                                                                      │    │
│ │     result.x = normalized_x * rescaled_magnitude;                    │    │
│ │     result.y = normalized_y * rescaled_magnitude;                    │    │
│ │     return result;                                                   │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // Usage:                                                            │    │
│ │ real32 raw_x = paxis_value(input, 0, PAXIS_LEFT_STICK_X);            │    │
│ │ real32 raw_y = paxis_value(input, 0, PAXIS_LEFT_STICK_Y);            │    │
│ │ Vec2 stick = apply_deadzone_circular(raw_x, raw_y, 0.2f);            │    │
│ │ player.vel_x = stick.x * player.speed;                               │    │
│ │ player.vel_y = stick.y * player.speed;                               │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│ DIAGRAM - Why circular is better:                                           │
│                                                                             │
│     Square Dead Zone:          Circular Dead Zone:                          │
│     ┌───────────────┐          ┌───────────────┐                            │
│     │   ┌───────┐   │          │     ╭───╮     │                            │
│     │   │ DEAD  │   │          │    ╱     ╲    │                            │
│     │   │       │   │          │   │ DEAD  │   │                            │
│     │   └───────┘   │          │    ╲     ╱    │                            │
│     │               │          │     ╰───╯     │                            │
│     └───────────────┘          └───────────────┘                            │
│     Corners have NO            Uniform dead zone                            │
│     dead zone! Drift!          in all directions                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 7: Keyboard Ghosting and Rollover                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Player presses W+A+Space but only W+A registers. Certain key combinations   │
│ don't work. Works on some keyboards but not others.                         │
│                                                                             │
│ ROOT CAUSE:                                                                 │
│ Cheap keyboards have limited "rollover" - can only detect N simultaneous    │
│ keys (often 2-3). This is a HARDWARE limitation, not your code!             │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Some key combos don't work                                                │
│ - Works on gaming keyboard, fails on laptop                                 │
│ - Inconsistent behavior across users                                        │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // You can't FIX this, but you can MITIGATE it:                      │    │
│ │                                                                      │    │
│ │ // 1. Design controls that don't require problematic combos          │    │
│ │ //    BAD:  W + A + Space + Shift (4 keys!)                          │    │
│ │ //    GOOD: W + Space (2 keys, almost always works)                  │    │
│ │                                                                      │    │
│ │ // 2. Offer alternative bindings using different key groups          │    │
│ │ //    WASD keys often conflict. Arrow keys + Ctrl usually work.      │    │
│ │                                                                      │    │
│ │ // 3. Allow gamepad as alternative (no ghosting!)                    │    │
│ │                                                                      │    │
│ │ // 4. Document in settings                                           │    │
│ │ printf("Note: Some keyboards can't detect 3+ simultaneous keys.\n"); │    │
│ │ printf("If controls feel unresponsive, try using a gamepad.\n");     │    │
│ │                                                                      │    │
│ │ // 5. Test with common problematic combos during development         │    │
│ │ //    W+A+S (forms a triangle on matrix - often fails)               │    │
│ │ //    Q+W+E (row of keys - usually OK)                               │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 8: Input Recording Determinism                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ You record a playthrough, but when you play it back, the game desyncs.      │
│ Player dies at a different spot. Enemies are in different positions.        │
│                                                                             │
│ ROOT CAUSES:                                                                │
│ 1. Not recording frame-perfect input (timing drift)                         │
│ 2. Floating-point non-determinism across machines                           │
│ 3. Random number generator not seeded consistently                          │
│ 4. External state (time of day, file system, etc.)                          │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // Record input BY FRAME NUMBER, not by timestamp                    │    │
│ │ typedef struct {                                                     │    │
│ │     uint64_t frame_number;     // THE KEY: frame, not time!          │    │
│ │     PhysicalInputState input;  // Complete input state               │    │
│ │ } InputRecordFrame;                                                  │    │
│ │                                                                      │    │
│ │ // During recording:                                                 │    │
│ │ void record_frame(InputRecording* rec, uint64_t frame,               │    │
│ │                   PhysicalInputState* input) {                       │    │
│ │     InputRecordFrame* f = &rec->frames[rec->count++];                │    │
│ │     f->frame_number = frame;                                         │    │
│ │     memcpy(&f->input, input, sizeof(PhysicalInputState));            │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // During playback:                                                  │    │
│ │ PhysicalInputState* get_playback_input(InputRecording* rec,          │    │
│ │                                         uint64_t frame) {            │    │
│ │     // Binary search for frame (recordings may skip unchanged frames)│    │
│ │     int idx = find_frame(rec, frame);                                │    │
│ │     if (idx >= 0) {                                                  │    │
│ │         return &rec->frames[idx].input;                              │    │
│ │     }                                                                │    │
│ │     // If no entry for this frame, use previous frame's state        │    │
│ │     return get_playback_input(rec, frame - 1);                       │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // CRITICAL: Seed RNG at recording start, save seed                  │    │
│ │ typedef struct {                                                     │    │
│ │     uint32_t rng_seed;        // MUST save this!                     │    │
│ │     InputRecordFrame* frames;                                        │    │
│ │     int count;                                                       │    │
│ │ } InputRecording;                                                    │    │
│ │                                                                      │    │
│ │ void start_recording(InputRecording* rec) {                          │    │
│ │     rec->rng_seed = (uint32_t)time(NULL);                            │    │
│ │     srand(rec->rng_seed);     // Seed now                            │    │
│ │     rec->count = 0;                                                  │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ void start_playback(InputRecording* rec) {                           │    │
│ │     srand(rec->rng_seed);     // Same seed = same random numbers!    │    │
│ │ }                                                                    │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│ ADDITIONAL REQUIREMENTS FOR DETERMINISM:                                    │
│ ├── Fixed timestep game loop (no variable delta time)                       │
│ ├── Avoid time-of-day or real-world clock in game logic                     │
│ ├── Avoid floating-point accumulation (use fixed-point for physics)         │
│ ├── Avoid order-dependent iteration (sort entities by ID)                   │
│ └── Compile with consistent FP settings across machines                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 9: Combo Detection Window                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Player tries to do ↓↘→+Punch (hadouken) but it doesn't register.            │
│ Or it triggers accidentally. Window feels too tight or too loose.           │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Combos never trigger (window too tight)                                   │
│ - Combos trigger when not intended (window too loose)                       │
│ - Different players have wildly different success rates                     │
│ - Combos work at 60 FPS but fail at 30 FPS                                  │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // Use TIME, not FRAMES, for combo windows                           │    │
│ │ typedef struct {                                                     │    │
│ │     PhysicalKey key;           // Or button                          │    │
│ │     real64 timestamp;          // When it was pressed                │    │
│ │ } InputHistoryEntry;                                                 │    │
│ │                                                                      │    │
│ │ typedef struct {                                                     │    │
│ │     InputHistoryEntry entries[64];  // Ring buffer                   │    │
│ │     int head;                                                        │    │
│ │     int count;                                                       │    │
│ │ } InputHistory;                                                      │    │
│ │                                                                      │    │
│ │ // Check for combo (e.g., ↓ ↘ → + P within 300ms)                    │    │
│ │ bool check_hadouken(InputHistory* history, real64 current_time) {    │    │
│ │     real64 MAX_COMBO_TIME = 0.300;  // 300ms window                  │    │
│ │                                                                      │    │
│ │     // Find the required sequence in reverse order                   │    │
│ │     int punch_idx = find_recent(history, PKEY_PUNCH, current_time,   │    │
│ │                                  MAX_COMBO_TIME);                    │    │
│ │     if (punch_idx < 0) return false;                                 │    │
│ │                                                                      │    │
│ │     real64 punch_time = history->entries[punch_idx].timestamp;       │    │
│ │                                                                      │    │
│ │     int right_idx = find_recent(history, PKEY_RIGHT, punch_time,     │    │
│ │                                  MAX_COMBO_TIME);                    │    │
│ │     if (right_idx < 0) return false;                                 │    │
│ │                                                                      │    │
│ │     // For diagonal, check if DOWN and RIGHT overlapped              │    │
│ │     int down_idx = find_recent(history, PKEY_DOWN,                   │    │
│ │                                 history->entries[right_idx].timestamp│    │
│ │                                 MAX_COMBO_TIME);                     │    │
│ │     if (down_idx < 0) return false;                                  │    │
│ │                                                                      │    │
│ │     // Verify order: DOWN before RIGHT+DOWN before RIGHT before PUNCH│    │
│ │     // (Simplified - real implementation needs overlap detection)    │    │
│ │     return true;                                                     │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // PRO TIP: Add "leniency" for close-enough inputs                   │    │
│ │ // ↓ then → is "close enough" to ↓↘→ for most players                │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│ TUNING GUIDELINES:                                                          │
│ ├── Casual game: 400-500ms window, high leniency                            │
│ ├── Standard game: 200-300ms window, medium leniency                        │
│ ├── Competitive game: 100-200ms window, exact inputs required               │
│ └── Always test with actual players, not just yourself!                     │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 10: Backend Parity                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Input works perfectly on X11 but behaves differently on Raylib.             │
│ Or vice versa. Subtle differences cause bugs.                               │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - "It works on my machine!"                                                 │
│ - Key repeat behaves differently                                            │
│ - Gamepad axis ranges are different (0-255 vs -32768 to 32767)              │
│ - Events arrive in different order                                          │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // 1. Create a test suite that runs on ALL backends                  │    │
│ │ void test_input_backend(PhysicalInputState* input) {                 │    │
│ │     // Automated tests                                               │    │
│ │     assert(pkey_pressed(input, PKEY_UNKNOWN) == false);              │    │
│ │     assert(PKEY_A >= 0 && PKEY_A < PKEY_COUNT);                      │    │
│ │     // ... more tests                                                │    │
│ │                                                                      │    │
│ │     // Interactive test mode                                         │    │
│ │     printf("Press 'A' key...\n");                                    │    │
│ │     while (!pkey_just_pressed(input, PKEY_A)) {                      │    │
│ │         poll_input(input);                                           │    │
│ │     }                                                                │    │
│ │     printf("✓ 'A' key detected\n");                                  │    │
│ │     // ... test all inputs                                           │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // 2. Normalize values at the backend boundary                       │    │
│ │ // X11 joystick: -32767 to 32767                                     │    │
│ │ // Raylib: Depends on backend                                        │    │
│ │ // SDL: -32768 to 32767                                              │    │
│ │ // YOUR PhysicalInputState: ALWAYS -1.0 to 1.0                       │    │
│ │                                                                      │    │
│ │ real32 normalize_axis(int raw, int min, int max) {                   │    │
│ │     return (2.0f * (raw - min) / (max - min)) - 1.0f;                │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // 3. Handle key repeat consistently (continued)                     │    │
│ │ void x11_handle_key_event(XEvent* event, PhysicalInputState* input) {│    │
│ │     // Ignore auto-repeat: check if next event is KeyRelease         │    │
│ │     if (event->type == KeyPress && XPending(display) > 0) {          │    │
│ │         XEvent next;                                                 │    │
│ │         XPeekEvent(display, &next);                                  │    │
│ │         if (next.type == KeyRelease &&                               │    │
│ │             next.xkey.time == event->xkey.time &&                    │    │
│ │             next.xkey.keycode == event->xkey.keycode) {              │    │
│ │             // This is auto-repeat, skip both events                 │    │
│ │             XNextEvent(display, &next);  // Consume the release      │    │
│ │             return;  // Ignore the press                             │    │
│ │         }                                                            │    │
│ │     }                                                                │    │
│ │     // Real key press, handle it                                     │    │
│ │     PhysicalKey pkey = x11_keysym_to_physical(keysym);               │    │
│ │     input->keys[pkey] = (event->type == KeyPress);                   │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // 4. Create a backend conformance checklist                         │    │
│ │ /*                                                                   │    │
│ │  * BACKEND CONFORMANCE CHECKLIST:                                    │    │
│ │  * □ All PhysicalKey values map correctly                            │    │
│ │  * □ All PhysicalButton values map correctly                         │    │
│ │  * □ Axis values normalized to -1.0 to 1.0                           │    │
│ │  * □ Key repeat filtered out                                         │    │
│ │  * □ just_pressed works (requires prev frame state)                  │    │
│ │  * □ Gamepad hot-plug handled                                        │    │
│ │  * □ All events drained each frame                                   │    │
│ │  * □ Focus loss clears all keys                                      │    │
│ │  */                                                                  │    │
│ │                                                                      │    │
│ │ // 5. Clear state on focus loss (IMPORTANT!)                         │    │
│ │ void handle_focus_lost(PhysicalInputState* input) {                  │    │
│ │     // When window loses focus, release all keys                     │    │
│ │     // Otherwise: user alt-tabs, 'W' stays "pressed" forever!        │    │
│ │     memset(input->keys, 0, sizeof(input->keys));                     │    │
│ │     for (int i = 0; i < MAX_GAMEPADS; i++) {                         │    │
│ │         memset(input->gamepads[i].buttons, 0,                        │    │
│ │                sizeof(input->gamepads[i].buttons));                  │    │
│ │         // Note: Don't clear axes - they return to center naturally  │    │
│ │     }                                                                │    │
│ │ }                                                                    │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│ TESTING MATRIX:                                                             │
│ ┌───────────────┬──────────┬──────────┬──────────┬──────────┐              │
│ │ Feature       │ X11      │ Raylib   │ SDL      │ Status   │              │
│ ├───────────────┼──────────┼──────────┼──────────┼──────────┤              │
│ │ Keyboard      │ ✓        │ ✓        │ -        │ Pass     │              │
│ │ Key repeat    │ Filtered │ N/A      │ -        │ Pass     │              │
│ │ Gamepad       │ ✓        │ ✓        │ -        │ Pass     │              │
│ │ Hot-plug      │ ✓        │ ?        │ -        │ Test!    │              │
│ │ Dead zones    │ ✓        │ ✓        │ -        │ Pass     │              │
│ │ Focus loss    │ ✓        │ ✓        │ -        │ Pass     │              │
│ └───────────────┴──────────┴──────────┴──────────┴──────────┘              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 11: Configuration File Format                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ How do you save/load key bindings? What format? What if user edits file     │
│ and corrupts it? What if you add new actions in an update?                  │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - Crash on corrupted config file                                            │
│ - New actions have no bindings after update                                 │
│ - Binary format is uneditable                                               │
│ - Text format is verbose and error-prone                                    │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // Use simple text format with versioning                            │    │
│ │                                                                      │    │
│ │ // controls.cfg:                                                     │    │
│ │ // ────────────────────────────────                                  │    │
│ │ // # Handmade Hero Controls v1                                       │    │
│ │ // # Lines starting with # are comments                              │    │
│ │ // # Format: ACTION = KEY [, KEY2, KEY3...]                          │    │
│ │ //                                                                   │    │
│ │ // move_up = W, GAMEPAD_DPAD_UP, GAMEPAD_LSTICK_UP                   │    │
│ │ // move_down = S, GAMEPAD_DPAD_DOWN, GAMEPAD_LSTICK_DOWN             │    │
│ │ // move_left = A, GAMEPAD_DPAD_LEFT, GAMEPAD_LSTICK_LEFT             │    │
│ │ // move_right = D, GAMEPAD_DPAD_RIGHT, GAMEPAD_LSTICK_RIGHT          │    │
│ │ // attack = SPACE, GAMEPAD_A                                         │    │
│ │ // pause = ESCAPE, GAMEPAD_START                                     │    │
│ │                                                                      │    │
│ │ typedef struct {                                                     │    │
│ │     int version;                                                     │    │
│ │     ActionBinding bindings[MAX_ACTIONS];                             │    │
│ │     int binding_count;                                               │    │
│ │ } ControlsConfig;                                                    │    │
│ │                                                                      │    │
│ │ bool load_controls(const char* path, ControlsConfig* config) {       │    │
│ │     FILE* f = fopen(path, "r");                                      │    │
│ │     if (!f) {                                                        │    │
│ │         printf("No config file, using defaults\n");                  │    │
│ │         return false;  // Caller should apply defaults               │    │
│ │     }                                                                │    │
│ │                                                                      │    │
│ │     char line[256];                                                  │    │
│ │     while (fgets(line, sizeof(line), f)) {                           │    │
│ │         // Skip comments and empty lines                             │    │
│ │         if (line[0] == '#' || line[0] == '\n') continue;             │    │
│ │                                                                      │    │
│ │         // Parse "action = key1, key2, key3"                         │    │
│ │         char action_name[64];                                        │    │
│ │         char keys_str[192];                                          │    │
│ │         if (sscanf(line, "%63s = %191[^\n]",                         │    │
│ │                    action_name, keys_str) != 2) {                    │    │
│ │             printf("Warning: Invalid line: %s", line);               │    │
│ │             continue;  // Skip bad lines, don't crash!               │    │
│ │         }                                                            │    │
│ │                                                                      │    │
│ │         int action_id = action_name_to_id(action_name);              │    │
│ │         if (action_id < 0) {                                         │    │
│ │             printf("Warning: Unknown action: %s\n", action_name);    │    │
│ │             continue;  // Skip unknown actions (forward compat)      │    │
│ │         }                                                            │    │
│ │                                                                      │    │
│ │         // Parse comma-separated keys                                │    │
│ │         parse_key_list(keys_str, &config->bindings[action_id]);      │    │
│ │     }                                                                │    │
│ │                                                                      │    │
│ │     fclose(f);                                                       │    │
│ │     return true;                                                     │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // CRITICAL: Apply defaults for any action NOT in file               │    │
│ │ void apply_defaults_for_missing(ControlsConfig* config) {            │    │
│ │     for (int i = 0; i < ACTION_COUNT; i++) {                         │    │
│ │         if (config->bindings[i].source_count == 0) {                 │    │
│ │             // This action has no bindings - use default             │    │
│ │             apply_default_binding(config, i);                        │    │
│ │         }                                                            │    │
│ │     }                                                                │    │
│ │ }                                                                    │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│ KEY PRINCIPLES:                                                             │
│ ├── Never crash on bad input - warn and skip                                │
│ ├── Unknown actions are ignored (forward compatibility)                     │
│ ├── Missing actions get defaults (backward compatibility)                   │
│ ├── Human-readable format (users can edit)                                  │
│ ├── Comments explain format                                                 │
│ └── Version number for future format changes                                │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────────┐
│ CHALLENGE 12: Accessibility Considerations                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PROBLEM:                                                                    │
│ Some players can only use one hand. Some can't use mouse. Some need         │
│ longer response times. Default controls are unusable for them.              │
│                                                                             │
│ SYMPTOMS:                                                                   │
│ - "I can't play this game because..."                                       │
│ - Refund requests citing control issues                                     │
│ - Negative reviews about accessibility                                      │
│                                                                             │
│ SOLUTION:                                                                   │
│ ┌──────────────────────────────────────────────────────────────────────┐    │
│ │ // 1. Allow FULL key remapping (no hardcoded keys)                   │    │
│ │ // Even "pause" should be remappable                                 │    │
│ │                                                                      │    │
│ │ // 2. Support "toggle" mode for holds                                │    │
│ │ typedef struct {                                                     │    │
│ │     bool toggle_sprint;      // Press once to sprint, again to stop  │    │
│ │     bool toggle_aim;         // Press once to aim, again to stop     │    │
│ │     bool toggle_crouch;                                              │    │
│ │ } AccessibilityOptions;                                              │    │
│ │                                                                      │    │
│ │ bool is_sprinting(GameState* state, PhysicalInputState* input,       │    │
│ │                   AccessibilityOptions* opts) {                      │    │
│ │     if (opts->toggle_sprint) {                                       │    │
│ │         // Toggle mode: track state                                  │    │
│ │         if (action_just_pressed(map, ACTION_SPRINT)) {               │    │
│ │             state->sprint_toggled = !state->sprint_toggled;          │    │
│ │         }                                                            │    │
│ │         return state->sprint_toggled;                                │    │
│ │     } else {                                                         │    │
│ │         // Hold mode: direct input                                   │    │
│ │         return action_is_pressed(map, ACTION_SPRINT);                │    │
│ │     }                                                                │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ // 3. Auto-aim / aim assist options                                  │    │
│ │ typedef struct {                                                     │    │
│ │     real32 aim_assist_strength;  // 0.0 = off, 1.0 = full            │    │
│ │     real32 aim_slowdown;         // Slow cursor near targets         │    │
│ │ } AimAssistOptions;                                                  │    │
│ │                                                                      │    │
│ │ // 4. Input timing adjustments                                       │    │
│ │ typedef struct {                                                     │    │
│ │     real32 combo_window_multiplier;  // 1.0 = normal, 2.0 = easier   │    │
│ │     real32 input_buffer_time;        // Buffer inputs for N ms       │    │
│ │     bool simplified_combos;          // Allow easier input patterns  │    │
│ │ } TimingOptions;                                                     │    │
│ │                                                                      │    │
│ │ // 5. One-handed presets                                             │    │
│ │ void apply_one_handed_left_preset(ControlsConfig* config) {          │    │
│ │     // All controls on left side of keyboard + mouse                 │    │
│ │     bind(config, ACTION_MOVE_UP, PKEY_W);                            │    │
│ │     bind(config, ACTION_MOVE_DOWN, PKEY_S);                          │    │
│ │     bind(config, ACTION_ATTACK, PKEY_SPACE);                         │    │
│ │     bind(config, ACTION_SPECIAL, PKEY_Q);                            │    │
│ │     // ... etc                                                       │    │
│ │ }                                                                    │    │
│ │                                                                      │    │
│ │ void apply_one_handed_right_preset(ControlsConfig* config) {         │    │
│ │     // All controls on numpad + right side                           │    │
│ │     bind(config, ACTION_MOVE_UP, PKEY_NUMPAD_8);                     │    │
│ │     bind(config, ACTION_MOVE_DOWN, PKEY_NUMPAD_2);                   │    │
│ │     // ... etc                                                       │    │
│ │ }                                                                    │    │
│ └──────────────────────────────────────────────────────────────────────┘    │
│                                                                             │
│ ACCESSIBILITY CHECKLIST:                                                    │
│ ├── □ All actions remappable (no hardcoded keys)                            │
│ ├── □ Toggle options for all hold-to-activate actions                       │
│ ├── □ Adjustable timing windows                                             │
│ ├── □ One-handed control presets                                            │
│ ├── □ Colorblind-friendly UI (not input, but related)                       │
│ ├── □ Controller support (for those who can't use keyboard)                 │
│ ├── □ Keyboard-only support (for those who can't use mouse)                 │
│ └── □ Input repeat rate adjustable                                          │
│                                                                             │
│ RESOURCES:                                                                  │
│ ├── Game Accessibility Guidelines: https://gameaccessibilityguidelines.com/ │
│ ├── AbleGamers: https://ablegamers.org/                                     │
│ └── Xbox Accessibility Guidelines (Microsoft)                               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 6. Quick Reference: Implementation Order

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     RECOMMENDED IMPLEMENTATION ORDER                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PHASE 1: Foundation (Do Now - ~6 hours)                                     │
│ ══════════════════════════════════════                                      │
│                                                                             │
│ □ Step 1: Create PhysicalKey enum                                           │
│   └── File: engine/input/physical_input.h                                   │
│   └── Content: ~50 common keys + PKEY_UNKNOWN                               │
│                                                                             │
│ □ Step 2: Create PhysicalButton enum                                        │
│   └── Same file                                                             │
│   └── Content: Standard gamepad buttons (A, B, X, Y, etc.)                  │
│                                                                             │
│ □ Step 3: Create PhysicalAxis enum                                          │
│   └── Same file                                                             │
│   └── Content: LSTICK_X, LSTICK_Y, RSTICK_X, RSTICK_Y, triggers             │
│                                                                             │
│ □ Step 4: Create PhysicalInputState struct                                  │
│   └── Same file                                                             │
│   └── Content: keys[], keys_prev[], gamepads[]                              │
│                                                                             │
│ □ Step 5: Create helper functions                                           │
│   └── pkey_pressed(), pkey_just_pressed(), pkey_just_released()             │
│   └── paxis_value(), pbtn_pressed()                                         │
│                                                                             │
│ □ Step 6: Modify X11 keyboard.c                                             │
│   └── Add x11_keysym_to_physical() translation table                        │
│   └── Remove all GameInput/action knowledge                                 │
│   └── Set input->keys[pkey] instead of controller->move_up                  │
│                                                                             │
│ □ Step 7: Modify X11 joystick.c                                             │
│   └── Add linux_button_to_physical() translation                            │
│   └── Add linux_axis_to_physical() translation                              │
│   └── Fill PhysicalInputState instead of GameInput                          │
│                                                                             │
│ □ Step 8: Modify backend.c                                                  │
│   └── Use PhysicalInputState instead of GameInput                           │
│   └── Call physical_input_begin_frame() each frame                          │
│                                                                             │
│ □ Step 9: Create game-side mapping (in game.c)                              │
│   └── static inline bool input_move_up(PhysicalInputState* in)              │
│   └── Check PKEY_W || pbtn_pressed(PBTN_DPAD_UP) || stick_y < -0.5          │
│   └── Repeat for all game actions                                           │
│                                                                             │
│ □ Step 10: Test on X11 backend                                              │
│   └── Verify keyboard works                                                 │
│   └── Verify gamepad works                                                  │
│   └── Verify just_pressed works                                             │
│                                                                             │
│ □ Step 11: Port to Raylib backend (if applicable)                           │
│   └── Same physical_input.h                                                 │
│   └── Create raylib_key_to_physical() translation                           │
│   └── Test parity with X11                                                  │
│                                                                             │
│ MILESTONE: Backends are now game-agnostic! ✓                                │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PHASE 2: Binding System (When Needed - ~14 hours)                           │
│ ═════════════════════════════════════════════════                           │
│                                                                             │
│ TRIGGER: When you need:                                                     │
│ ├── User-configurable controls                                              │
│ ├── Multiple games on same engine                                           │
│ └── Settings menu with rebinding UI                                         │
│                                                                             │
│ □ Step 1: Create ActionBinding struct                                       │
│ □ Step 2: Create ActionMap struct                                           │
│ □ Step 3: Create action_bind() / action_unbind()                            │
│ □ Step 4: Create action_is_pressed() / action_just_pressed()                │
│ □ Step 5: Create game-specific action enum                                  │
│ □ Step 6: Create default bindings                                           │
│ □ Step 7: Create save/load functions                                        │
│ □ Step 8: Create rebinding UI                                               │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PHASE 3: Advanced Features (Future - As Needed)                             │
│ ════════════════════════════════════════════════                            │
│                                                                             │
│ □ Input Recording/Playback                                                  │
│   └── TRIGGER: When you need replay, testing, or demos                      │
│                                                                             │
│ □ Combo System                                                              │
│   └── TRIGGER: When making fighting/action game                             │
│                                                                             │
│ □ Touch/Gesture Support                                                     │
│   └── TRIGGER: When targeting mobile/touch                                  │
│                                                                             │
│ □ Network Input (Rollback)                                                  │
│   └── TRIGGER: When making online multiplayer                               │
│                                                                             │
│ □ Input Visualization/Debug                                                 │
│   └── TRIGGER: When debugging complex input issues                          │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 7. File Structure Reference

```
project/
├── engine/
│   ├── input/                          # ← NEW DIRECTORY
│   │   ├── physical_input.h            # PhysicalKey, PhysicalButton, etc.
│   │   ├── physical_input.c            # Helper implementations (if needed)
│   │   ├── action_types.h              # ActionBinding, ActionMap (Phase 2)
│   │   ├── action_system.c             # Binding system (Phase 2)
│   │   ├── input_recorder.h            # Recording/playback (Phase 3)
│   │   └── input_recorder.c            # (Phase 3)
│   │
│   ├── platform/
│   │   ├── x11/
│   │   │   ├── backend.c               # Uses PhysicalInputState
│   │   │   └── inputs/
│   │   │       ├── keyboard.c          # x11_keysym_to_physical()
│   │   │       └── joystick.c          # linux_button_to_physical()
│   │   │
│   │   └── raylib/
│   │       └── backend.c               # Uses same PhysicalInputState
│   │
│   └── game/
│       ├── input.h                     # Current GameInput (deprecated later)
│       └── input.c                     # (deprecated later)
│
└── handmadehero/                       # Your game
    ├── game.c                          # Game-specific input_*() helpers
    ├── game.h
    └── input_actions.h                 # ACTION_MOVE_UP, etc. (Phase 2)
```

## 8. Minimal PhysicalKey Example (Continued - Abbreviated)

```
// ... (continued from PhysicalGamepadState)

typedef struct {
    // Keyboard
    bool keys[PKEY_COUNT];
    bool keys_prev[PKEY_COUNT];

    // Gamepads
    PhysicalGamepadState gamepads[MAX_PHYSICAL_GAMEPADS];
} PhysicalInputState;

// ═══════════════════════════════════════════════════════════════════════════
// HELPER FUNCTIONS (inline for performance)
// ═══════════════════════════════════════════════════════════════════════════

static inline void physical_input_begin_frame(PhysicalInputState* state) {
    memcpy(state->keys_prev, state->keys, sizeof(state->keys));
    for (int i = 0; i < MAX_PHYSICAL_GAMEPADS; i++) {
        memcpy(state->gamepads[i].buttons_prev,
               state->gamepads[i].buttons,
               sizeof(state->gamepads[i].buttons));
    }
}

static inline bool pkey_pressed(PhysicalInputState* s, PhysicalKey k) {
    return (k >= 0 && k < PKEY_COUNT) ? s->keys[k] : false;
}

static inline bool pkey_just_pressed(PhysicalInputState* s, PhysicalKey k) {
    return pkey_pressed(s, k) && !s->keys_prev[k];
}

static inline bool pkey_just_released(PhysicalInputState* s, PhysicalKey k) {
    return !pkey_pressed(s, k) && s->keys_prev[k];
}

static inline bool pbtn_pressed(PhysicalInputState* s, int pad, PhysicalButton b) {
    if (pad < 0 || pad >= MAX_PHYSICAL_GAMEPADS) return false;
    if (!s->gamepads[pad].connected) return false;
    return (b >= 0 && b < PBTN_COUNT) ? s->gamepads[pad].buttons[b] : false;
}

static inline real32 paxis_value(PhysicalInputState* s, int pad, PhysicalAxis a) {
    if (pad < 0 || pad >= MAX_PHYSICAL_GAMEPADS) return 0.0f;
    if (!s->gamepads[pad].connected) return 0.0f;
    return (a >= 0 && a < PAXIS_COUNT) ? s->gamepads[pad].axes[a] : 0.0f;
}

#endif // DE100_ENGINE_INPUT_PHYSICAL_INPUT_H
```

physical_input.h

## 9. Backend Translation Table Example (X11)

```
// Translation table: X11 KeySym → PhysicalKey
// Add entries as needed. Unknown keys return PKEY_UNKNOWN.

static PhysicalKey x11_keysym_to_physical(KeySym keysym) {
    switch (keysym) {
        // Letters
        case XK_a: case XK_A: return PKEY_A;
        case XK_w: case XK_W: return PKEY_W;
        case XK_s: case XK_S: return PKEY_S;
        case XK_d: case XK_D: return PKEY_D;
        // ... add more as needed

        // Navigation
        case XK_Up:    return PKEY_UP;
        case XK_Down:  return PKEY_DOWN;
        case XK_Left:  return PKEY_LEFT;
        case XK_Right: return PKEY_RIGHT;

        // Controls
        case XK_space:  return PKEY_SPACE;
        case XK_Escape: return PKEY_ESCAPE;
        case XK_Return: return PKEY_ENTER;

        default: return PKEY_UNKNOWN;
    }
}

// In event handler:
void x11_handle_key_event(XEvent* event, PhysicalInputState* input) {
    KeySym keysym = XLookupKeysym(&event->xkey, 0);
    PhysicalKey pkey = x11_keysym_to_physical(keysym);

    if (pkey != PKEY_UNKNOWN) {
        input->keys[pkey] = (event->type == KeyPress);
    }
}
```

keyboard.c

## 10. Game-Side Input Mapping Example

```
// Game-specific input helpers - knows about PhysicalKey but defines game actions

static inline bool input_move_up(PhysicalInputState* in) {
    return pkey_pressed(in, PKEY_W) ||
           pkey_pressed(in, PKEY_UP) ||
           pbtn_pressed(in, 0, PBTN_DPAD_UP) ||
           paxis_value(in, 0, PAXIS_LEFT_STICK_Y) < -0.5f;
}

static inline bool input_move_down(PhysicalInputState* in) {
    return pkey_pressed(in, PKEY_S) ||
           pkey_pressed(in, PKEY_DOWN) ||
           pbtn_pressed(in, 0, PBTN_DPAD_DOWN) ||
           paxis_value(in, 0, PAXIS_LEFT_STICK_Y) > 0.5f;
}

static inline bool input_attack(PhysicalInputState* in) {
    return pkey_just_pressed(in, PKEY_SPACE) ||
           pbtn_just_pressed(in, 0, PBTN_A);
}

// In game update:
void game_update(PhysicalInputState* input) {
    if (input_move_up(input))   player.y -= speed;
    if (input_move_down(input)) player.y += speed;
    if (input_attack(input))    do_attack();
}
```

game.c

## 11. Summary: The Key Insight

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          THE FUNDAMENTAL PATTERN                            │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│                                                                             │
│    ┌─────────────┐      ┌─────────────────┐      ┌─────────────────┐       │
│    │   Backend   │      │  Physical Input │      │   Game Code     │       │
│    │   (X11)     │ ───► │     State       │ ───► │   (Actions)     │       │
│    └─────────────┘      └─────────────────┘      └─────────────────┘       │
│                                                                             │
│    XK_w ──────────────► PKEY_W ───────────────► input_move_up()            │
│    XK_space ──────────► PKEY_SPACE ───────────► input_attack()             │
│    Button 0 ──────────► PBTN_A ───────────────► input_attack()             │
│                                                                             │
│                                                                             │
│    BACKEND RESPONSIBILITY:          GAME RESPONSIBILITY:                    │
│    ─────────────────────────        ────────────────────                    │
│    • Translate platform keys        • Define what actions exist             │
│      to PhysicalKey enum            • Map physical inputs to actions        │
│    • Fill PhysicalInputState        • Handle game-specific logic            │
│    • NO game knowledge!             • NO platform knowledge!                │
│                                                                             │
│                                                                             │
│    RESULT:                                                                  │
│    ────────                                                                 │
│    ✅ Backend can be reused for ANY game                                    │
│    ✅ Game can run on ANY backend                                           │
│    ✅ Clean separation of concerns                                          │
│    ✅ Easy to add new platforms                                             │
│    ✅ Easy to add new games                                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 12. When to Implement Each Phase

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         IMPLEMENTATION TRIGGERS                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PHASE 1: Physical Input Layer                                               │
│ ═════════════════════════════                                               │
│ WHEN: Now (you already have the circular dependency problem)                │
│ TIME: ~4-6 hours                                                            │
│ BENEFIT: Backends become game-agnostic, clean architecture                  │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ PHASE 2: Action Binding System                                              │
│ ══════════════════════════════                                              │
│ WHEN: You need ANY of these:                                                │
│   • User-remappable controls                                                │
│   • Settings menu with key binding                                          │
│   • Multiple games on same engine                                           │
│   • Saving/loading control preferences                                      │
│ TIME: ~8-12 hours                                                           │
│                                                                             │
│ ─────────────────────────────────────────────────────────────────────────── │
│                                                                             │
│ PHASE 3: Advanced Features                                                  │
│ ══════════════════════════                                                  │
│ WHEN: Specific game requirements demand them:                               │
│   • Input Recording → Replay system, automated testing                      │
│   • Combo System → Fighting game, action game                               │
│   • Network Input → Online multiplayer                                      │
│ TIME: Varies (4-20+ hours each)                                             │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 13. Final Checklist

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    IMPLEMENTATION CHECKLIST                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ PHASE 1 CHECKLIST:                                                          │
│ □ Created engine/input/physical_input.h                                     │
│ □ Defined PhysicalKey enum (at least keys you use)                          │
│ □ Defined PhysicalButton and PhysicalAxis enums                             │
│ □ Defined PhysicalInputState struct                                         │
│ □ Implemented helper functions (pkey_pressed, etc.)                         │
│ □ Created X11 translation table (x11_keysym_to_physical)                    │
│ □ Modified keyboard.c to use PhysicalInputState                             │
│ □ Modified joystick.c to use PhysicalInputState                             │
│ □ Modified backend.c to use PhysicalInputState                              │
│ □ Created game-side input helpers (input_move_up, etc.)                     │
│ □ Tested keyboard input works                                               │
│ □ Tested gamepad input works                                                │
│ □ Tested just_pressed/just_released works                                   │
│ □ Tested focus loss clears input state                                      │
│                                                                             │
│ VERIFICATION:                                                               │
│ □ Backend code has ZERO references to game actions                          │
│ □ Backend code has ZERO includes of game headers                            │
│ □ Game code has ZERO references to XK_* or platform keycodes                │
│ □ No circular dependencies                                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 14. Casey's Perspective on This

"The input layer is one of the simplest parts of the platform abstraction. You're just moving bits from one place to another. The complexity comes from making it extensible without overengineering it.

Start with what you need today. You don't need a full rebinding system on Day 16. You need W to move up. Build that. When you need rebinding, you'll understand the problem better and build a better solution.

The key insight is the separation point. Platform code knows about keycodes. Game code knows about actions. They meet in the middle at a simple struct of booleans and floats. That's it. Don't overthink it."
