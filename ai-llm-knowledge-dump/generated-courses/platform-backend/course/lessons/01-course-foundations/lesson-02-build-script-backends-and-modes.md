# Lesson 02: Build Script, Backend Selection, and Compile Personality

## Frontmatter

- Module: `01-course-foundations`
- Lesson: `02`
- Status: Required
- Target file:
  - `build-dev.sh`
- Target symbols:
  - `--backend=x11|raylib`
  - `--bench=N`
  - `--stress=N`
  - `--force-scene=N`
  - `--lock-scene`
  - `SOURCES`
  - `BASE_FLAGS`
  - `BACKEND_LIBS`
  - `RUN_AFTER_BUILD`

## Observable Outcome

By the end of this lesson, you should be able to look at a build command and explain all of the following without hand-waving:

1. Which backend shell will be compiled.
2. Whether you are making a debug-learning build or a timing-oriented bench build.
3. Which options change the binary itself.
4. Which options only change the post-build workflow.

If Lesson 01 taught you where the runtime lives, Lesson 02 teaches you what kind of runtime binary you are actually creating.

## Prerequisite Bridge

Lesson 01 established the live runtime spine.

This lesson zooms in on the first link in that chain:

```text
build-dev.sh
```

You do not need Bash expertise.

You only need one idea:

```text
a shell script is a small program that computes and runs another command
```

In this course, that command is a `clang` build command.

## Why This Lesson Exists

In low-level work, many confusing bugs are not really code bugs at first.

They are build-identity bugs.

The learner says:

```text
the game behaves like this
```

when the truthful statement is:

```text
the X11 bench build with these compile-time defines behaves like this
```

If you do not know what executable you compiled, you do not really know what behavior you are observing.

That is why the build script belongs inside the tutorial, not outside it.

## Step 1: Read the Script as a Pipeline

Do not read `build-dev.sh` as a bag of shell syntax.

Read it as a pipeline:

```text
CLI flags
  -> change shell variables
  -> choose compile personality
  -> choose shared source set
  -> choose backend-specific source files and libraries
  -> build one clang command
  -> optionally run the binary
```

Once that picture is stable, every later detail fits somewhere predictable.

## Step 2: Understand the Default State

The script begins with these defaults:

```bash
BACKEND="raylib"
RUN_AFTER_BUILD=false
BENCH_DURATION_S=""
STRESS_RECTS=""
FORCE_SCENE_INDEX=""
LOCK_SCENE=0
```

These lines define the baseline build identity.

### `BACKEND="raylib"`

- If you pass no backend flag, Raylib is the default shell.
- That makes the easier onboarding path the default learner experience.

### `RUN_AFTER_BUILD=false`

- Build and run are separate by default.
- This changes workflow, not program contents.

### Empty-string feature values

```bash
BENCH_DURATION_S=""
STRESS_RECTS=""
FORCE_SCENE_INDEX=""
```

An empty string here means the feature is disabled.

This is a simple and common shell pattern:

```text
empty string = feature off
non-empty string = feature on with a value
```

### `LOCK_SCENE=0`

- Numeric zero means the scene lock is off.
- Later, `1` turns it on.

### What build do you get with no flags?

Without extra options, the default build is:

```text
Raylib backend
debug-learning personality
sanitizers on
no benchmark duration
no stress rectangles
no scene override
build only, no auto-run
```

That is the baseline teaching executable.

## Step 3: Read the Argument Parser Like a State Machine

Here is the parser core:

```bash
while [[ $# -gt 0 ]]; do
    case "$1" in
        --backend=*)
            BACKEND="${1#*=}"
        ;;
        --bench=*)
            BENCH_DURATION_S="${1#*=}"
        ;;
        --stress=*)
            STRESS_RECTS="${1#*=}"
        ;;
        --force-scene=*)
            FORCE_SCENE_INDEX="${1#*=}"
        ;;
        --lock-scene)
            LOCK_SCENE=1
        ;;
        -r|--run)
            RUN_AFTER_BUILD=true
        ;;
        --help|-h)
            echo "Usage: $0 [--backend=x11|raylib] [--bench=N] [--stress=N] [-r|--run]"
            exit 0
        ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
        ;;
    esac
    shift
done
```

Treat this as a loop that repeatedly answers one question:

```text
what should this current command-line token change?
```

## Step 4: Learn the One Shell Trick That Matters Most

This expression appears several times:

```bash
"${1#*=}"
```

It means:

```text
take $1
remove everything from the front through the first '='
keep the value after '='
```

Examples:

```text
--backend=x11       -> x11
--bench=25          -> 25
--stress=343        -> 343
--force-scene=2     -> 2
```

This one small shell expansion is what turns `--name=value` flags into ordinary values the rest of the script can use.

If you understand nothing else about Bash in this lesson, understand this.

## Step 5: Separate Binary-Shaping Flags From Workflow Flags

Not every flag does the same kind of work.

### Flags that change the compiled binary

- `--backend=...`
- `--bench=...`
- `--stress=...`
- `--force-scene=...`
- `--lock-scene`

These eventually affect source selection, linked libraries, or compiler defines.

### Flags that change only workflow

- `-r`
- `--run`

These do not change the executable itself.

They only decide whether the finished binary gets executed immediately after a successful build.

This distinction is extremely important.

If you blur it, you will misdiagnose behavior later.

## Step 6: Read `SOURCES` as Shared Runtime Architecture

The build script defines the shared source set like this:

```bash
RENDER_COORD_FLAG="-DRENDER_COORD_MODE=1"
DEMO_SRC="src/game/demo_explicit.c"

SOURCES="src/utils/backbuffer.c src/utils/draw-shapes.c src/utils/draw-text.c $DEMO_SRC src/game/audio_demo.c src/game/main.c"
```

This is not just an inventory list.

It is an architecture statement.

### `RENDER_COORD_FLAG`

```bash
RENDER_COORD_FLAG="-DRENDER_COORD_MODE=1"
```

- This forces explicit render-coordinate mode at compile time.
- It is a build decision, not a key press or runtime toggle.

### `DEMO_SRC`

```bash
DEMO_SRC="src/game/demo_explicit.c"
```

- The older demo path still exists.
- It still matters for teaching and comparison.
- It is not the live top-level runtime facade.

### `SOURCES`

```bash
SOURCES="... src/game/audio_demo.c src/game/main.c"
```

Read this as:

```text
shared runtime core
compiled into both backends
```

That is why Lesson 01 could say the project is one shared runtime with two platform shells.

## Step 7: Dev Build vs Bench Build

The most important build split in the file is the compile personality split.

### Dev personality

If `--bench=N` is not present, the script uses:

```bash
BASE_FLAGS="-O0 -g -DDEBUG -fsanitize=address,undefined -Wall -Wextra -DBACKEND=$BACKEND $RENDER_COORD_FLAG"
```

Read each part with intent.

#### `-O0`

- Disable optimization.
- Better debugging.
- Easier correlation between source and observed behavior.

#### `-g`

- Emit debug symbols.
- Helps stack traces and debugging tools.

#### `-DDEBUG`

- Defines the `DEBUG` macro for conditional compilation.
- Makes debug-only paths available.

#### `-fsanitize=address,undefined`

- AddressSanitizer catches many memory errors.
- UndefinedBehaviorSanitizer catches many UB cases.
- This is exactly the sort of safety net beginners need.

#### `-Wall -Wextra`

- Ask the compiler to complain about suspicious code.
- Warnings are part of the teaching environment, not just polish.

#### `-DBACKEND=$BACKEND`

- Bake the backend identity into the compile.
- This is a compile-time constant, not runtime detection.

### Bench personality

If `--bench=N` is present, the script switches to:

```bash
BASE_FLAGS="-O2 -DNDEBUG -Wall -Wextra -DBACKEND=$BACKEND $RENDER_COORD_FLAG \
    -DENABLE_PERF -DBENCH_DURATION_S=${BENCH_DURATION_S}"
SOURCES="$SOURCES src/utils/perf.c"
```

This is not just "faster mode."

It is a different teaching personality.

#### `-O2`

- Turn on optimization.
- Better for timing-oriented observation.

#### `-DNDEBUG`

- Compile out debug-only assertions or branches guarded by `NDEBUG`.
- This changes behavior and diagnostics.

#### `-DENABLE_PERF`

- Include profiler-oriented code.

#### `-DBENCH_DURATION_S=...`

- Bake in an auto-exit duration for benchmark mode.

#### `src/utils/perf.c`

- The bench build needs extra profiler support code.

### Mental model

Do not think of `--bench=N` as a tiny option.

Think of it as:

```text
switch build personality from learn-and-debug
to measure-and-profile
```

## Step 8: Stress and Scene Overrides Are Compile-Time Teaching Controls

These blocks add more compile-time behavior switches:

```bash
if [[ -n "$STRESS_RECTS" ]]; then
    BASE_FLAGS="$BASE_FLAGS -DSTRESS_RECTS=${STRESS_RECTS}"
fi

if [[ -n "$FORCE_SCENE_INDEX" ]]; then
    BASE_FLAGS="$BASE_FLAGS -DCOURSE_FORCE_SCENE_INDEX=${FORCE_SCENE_INDEX}"
fi

if [[ "$LOCK_SCENE" == 1 ]]; then
    BASE_FLAGS="$BASE_FLAGS -DCOURSE_LOCK_SCENE=1"
fi
```

These matter because they are not runtime menu settings.

They are compile-time teaching switches.

### `STRESS_RECTS`

- Adds extra rectangles for stress testing and culling demonstrations.

### `COURSE_FORCE_SCENE_INDEX`

- Bakes in a specific scene selection.

### `COURSE_LOCK_SCENE=1`

- Prevents scene cycling away from the chosen lesson context.

Together, these are part of the course's teaching instrumentation.

## Step 9: Backend Branching

The backend-specific branch is where the script wraps the shared runtime in a concrete shell:

```bash
case "$BACKEND" in
    x11)
        BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX"
        SOURCES="$SOURCES src/platforms/x11/base.c src/platforms/x11/audio.c src/platforms/x11/main.c"
    ;;
    raylib)
        ...
        SOURCES="$SOURCES src/platforms/raylib/main.c"
    ;;
    *)
        echo "Error: Unknown backend '$BACKEND'" >&2
        echo "Available: x11, raylib" >&2
        exit 1
    ;;
esac
```

This section answers two questions:

1. Which extra source files must be compiled?
2. Which system or library dependencies must be linked?

### X11 branch

```bash
BACKEND_LIBS="-lm -lX11 -lxkbcommon -lasound -lGL -lGLX"
SOURCES="$SOURCES src/platforms/x11/base.c src/platforms/x11/audio.c src/platforms/x11/main.c"
```

- X11 needs extra platform code.
- It also needs explicit system libraries for windowing, keyboard handling, OpenGL, and ALSA.

### Raylib branch

```bash
SOURCES="$SOURCES src/platforms/raylib/main.c"
```

- Raylib uses a thinner backend shell here.
- The script still adjusts libraries depending on the detected OS.

### Unknown backend branch

```bash
echo "Error: Unknown backend '$BACKEND'" >&2
```

Good scripts fail loudly and early.

That is not just convenience. It prevents learners from silently building the wrong thing.

## Step 10: The Final Command

At the end, the script assembles and runs the real build:

```bash
BINARY="./build/game"
INCLUDE_FLAGS="-Isrc"

clang $BASE_FLAGS $INCLUDE_FLAGS -o "$BINARY" $SOURCES $BACKEND_LIBS
```

This is the moment where all the earlier choices collapse into one concrete build command.

Read it as:

```text
compiler flags
+ include path
+ output path
+ chosen sources
+ chosen libraries
= one executable
```

## Step 11: `--run` Is Workflow Only

The last block is:

```bash
if [[ "$RUN_AFTER_BUILD" == true ]]; then
    echo ""
    echo "Running"
    exec "$BINARY"
fi
```

This does not rebuild the binary differently.

It just changes what happens after a successful build.

This is worth repeating because it is such a common misunderstanding:

```text
--run changes workflow
it does not change program contents
```

## Worked Examples

### Example 1: Simple learner build

```bash
./build-dev.sh --backend=raylib
```

Meaning:

- Raylib shell
- debug personality
- sanitizers on
- no benchmark duration
- no stress rectangles
- no scene override
- build only

### Example 2: Measure culling behavior in X11

```bash
./build-dev.sh --backend=x11 --bench=20 --stress=343
```

Meaning:

- X11 shell
- optimized bench personality
- perf enabled
- auto-exit after 20 seconds
- compile-time stress-rect count set to 343

### Example 3: Lock the course into one scene and run immediately

```bash
./build-dev.sh --backend=raylib --force-scene=2 --lock-scene --run
```

Meaning:

- Raylib shell
- debug personality unless `--bench` is also present
- build-time scene override to scene index 2
- build-time scene lock enabled
- run immediately after successful build

## Visual Summary

```text
command-line flags
    |
    v
shell variables
    |
    +--> BASE_FLAGS
    +--> SOURCES
    +--> BACKEND_LIBS
    +--> RUN_AFTER_BUILD
    |
    v
clang build command
    |
    v
./build/game
    |
    +--> optional exec if --run was passed
```

## Common Beginner Mistakes

### Mistake 1: Treating all flags as runtime configuration

Many flags here are compile-time switches.

That means you cannot assume toggling them later at runtime is even possible.

### Mistake 2: Forgetting which build personality you are in

If a sanitizer report disappears in bench mode, that does not mean the bug is gone.

It may simply mean you switched to an optimized build without sanitizers.

### Mistake 3: Thinking `--run` affects the binary

It does not.

It only controls whether the script executes the finished binary.

### Mistake 4: Treating `SOURCES` like a random file list

It is not random.

It is the build script's summary of what counts as shared runtime core.

## Practice

Answer these before moving on:

1. Why is `--bench=N` more than just a timer flag?
2. Why is `--run` not comparable to `--stress=N`?
3. What does `${1#*=}` do when parsing `--force-scene=3`?
4. Which part of the script proves that both backends compile the same shared runtime core?

## Mini Exercises

### Exercise 1

Predict what changes between these two commands:

```bash
./build-dev.sh --backend=raylib
./build-dev.sh --backend=raylib --bench=15
```

Write down the differences in:

- optimization
- debug macros
- sanitizer usage
- extra sources
- intent of the build

### Exercise 2

Explain why this pair is not equivalent:

```bash
./build-dev.sh --run
./build-dev.sh --force-scene=1
```

One changes workflow. The other changes the compiled program.

## Troubleshooting Your Understanding

### "I can read the script, but I cannot tell what affects the binary"

Ask one question for every option:

```text
does this eventually alter BASE_FLAGS, SOURCES, or BACKEND_LIBS?
```

If yes, it changes the binary.

If not, it is probably workflow.

### "I keep forgetting whether a value is runtime or compile-time"

Look for `-DNAME=value` inside `BASE_FLAGS`.

That is a strong clue you are looking at compile-time behavior.

### "The Bash syntax still feels unfamiliar"

That is fine.

You do not need to become a shell expert here.

You only need to be able to explain the build pipeline truthfully.

## Recap

You now know that `build-dev.sh` is responsible for:

1. Parsing course-specific CLI flags.
2. Choosing a compile personality.
3. Choosing shared runtime sources.
4. Wrapping those shared sources in a backend-specific shell.
5. Producing one executable build command.
6. Optionally running that executable.

That is enough build literacy to keep the rest of the course grounded in reality.

## Next Lesson

The next question is:

```text
What shared language do both backend shells use once the program is built?
```

Lesson 03 answers that by unpacking `src/platform.h`, especially:

- shared macros
- `PlatformGameProps`
- startup and cleanup helpers
- letterbox helpers
- audio function prototypes
